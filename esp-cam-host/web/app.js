// Interface da impressora (servida pela ESP32-S3-CAM).
// Estado e console chegam pelo WebSocket /ws; comandos vao pelo mesmo canal.
'use strict';

const $ = (id) => document.getElementById(id);
const S = { token: '', ws: null, estado: null, vivo: false, passo: 10,
            hist: [], histPos: 0, serie: [], tentativas: 0 };

// ---------------------------------------------------------------- utilidades
function aviso(txt, erro) {
  const a = $('aviso');
  a.textContent = txt;
  a.className = 'aviso mostra' + (erro ? ' erro' : '');
  clearTimeout(aviso.t);
  aviso.t = setTimeout(() => a.classList.remove('mostra'), 2600);
}
const br = (v, n = 1) => (v == null || isNaN(v)) ? '--' : Number(v).toFixed(n).replace('.', ',');

function envia(gcode) {
  if (!S.ws || S.ws.readyState !== 1) { aviso('Sem conexão com a placa', true); return false; }
  S.ws.send(gcode);
  return true;
}

// ---------------------------------------------------------------- conexao
async function sessao() {
  const r = await fetch('/api/sessao', { cache: 'no-store' });
  if (!r.ok) throw new Error('sessão ' + r.status);
  S.token = (await r.json()).token;
}

function conecta() {
  const ws = new WebSocket(`ws://${location.host}/ws?t=${S.token}`);
  S.ws = ws;
  ws.onopen = () => { S.tentativas = 0; $('log').textContent = ''; };
  ws.onmessage = (ev) => {
    let m;
    try { m = JSON.parse(ev.data); } catch (_) { return; }
    if (m.t === 'e') { atualiza(m); if (window.Nivel) Nivel.estado(m); if (window.Arquivos) Arquivos.estado(m); }
    else if (m.t === 'l') { linhaConsole(m); if (window.Nivel) Nivel.linha(m); }
  };
  ws.onclose = () => {
    chip('chipTb', 'erro', 'sem conexão');
    $('subTitulo').textContent = 'reconectando…';
    const espera = Math.min(10000, 1000 * 2 ** S.tentativas++);
    setTimeout(async () => {
      try { await sessao(); } catch (_) { /* a placa pode estar reiniciando */ }
      conecta();
    }, espera);
  };
}

// ---------------------------------------------------------------- estado
function chip(id, classe, texto) {
  const c = $(id);
  c.className = 'chip ' + classe;
  c.textContent = texto;
}

function atualiza(e) {
  S.estado = e;
  const tb = e.tb;
  if (!tb.usb) chip('chipTb', 'erro', 'TinyBee desconectada');
  else if (!tb.resp) chip('chipTb', 'aviso', 'TinyBee sem resposta');
  else chip('chipTb', tb.ocupada ? 'aviso' : 'ok', tb.ocupada ? 'TinyBee ocupada' : 'TinyBee ok');
  chip('chipRede', e.rede.ip ? 'ok' : 'aviso',
       e.rede.ip ? `Wi-Fi ${e.rede.rssi} dBm` : (e.rede.ap ? 'modo AP' : 'sem Wi-Fi'));
  chip('chipSd', e.sd.ok ? 'ok' : 'aviso',
       e.sd.ok ? `SD ${br(e.sd.livreMB / 1024, 1)} GB livres` : 'sem SD');
  $('subTitulo').textContent = tb.fw ? tb.fw : (tb.usb ? 'aguardando a TinyBee' : 'TinyBee desconectada');

  temp('Bico', tb.bico, tb.bicoAlvo, tb.bicoPwm);
  temp('Mesa', tb.mesa, tb.mesaAlvo, tb.mesaPwm);
  $('pX').textContent = br(tb.x, 2); $('pY').textContent = br(tb.y, 2);
  $('pZ').textContent = br(tb.z, 2); $('pE').textContent = br(tb.e, 2);
  $('pVel').textContent = tb.vel + '%';
  $('pFluxo').textContent = tb.fluxo + '%';
  $('pComp').textContent = tb.comp === 1 ? 'ligada' : tb.comp === 0 ? 'desligada' : '?';
  $('pFila').textContent = tb.fila;
  $('dicaFil').textContent = tb.bico < 170 ? 'bico frio' : '';

  // grafico: um ponto por segundo, 10 minutos
  S.serie.push([tb.bico, tb.bicoAlvo, tb.mesa, tb.mesaAlvo]);
  if (S.serie.length > 600) S.serie.shift();
  if (document.querySelector('#painel.ativa')) desenhaGrafico();

  infoSistema(e);
}

function temp(nome, atual, alvo, pwm) {
  $('t' + nome).textContent = br(atual, 1);
  $('a' + nome).textContent = alvo > 0 ? br(alvo, 0) + ' °C' : 'desligado';
  $('pwm' + nome).textContent = alvo > 0 || pwm > 0 ? `potência ${Math.round(pwm / 1.27)}%` : '';
  $('barra' + nome).style.width = Math.min(100, pwm / 1.27) + '%';
  const c = $('cartao' + nome);
  c.classList.toggle('aquecendo', alvo > 0 && Math.abs(atual - alvo) > 3);
  c.classList.toggle('no-alvo', alvo > 0 && Math.abs(atual - alvo) <= 3);
}

function infoSistema(e) {
  const tb = e.tb, horas = Math.floor(e.sis.seg / 3600), min = Math.floor(e.sis.seg % 3600 / 60);
  const par = (k, v) => `<div><span>${k}</span><b>${v}</b></div>`;
  $('infoTb').innerHTML =
    par('Ligação', tb.usb ? (tb.resp ? 'USB, respondendo' : 'USB, sem resposta') : 'desconectada') +
    par('Firmware', tb.fw || '--') +
    par('Área (X × Y × Z)', tb.max[0] ? `${br(tb.min[0], 0)}–${br(tb.max[0], 0)} × ${br(tb.min[1], 0)}–${br(tb.max[1], 0)} × ${br(tb.max[2], 0)} mm` : '--');
  $('infoEsp').innerHTML =
    par('Câmera', `${e.cam.sensor} ${e.cam.larg}×${e.cam.alt} (${e.cam.ligada ? 'ligada' : 'em repouso'})`) +
    par('Cartão SD', e.sd.ok ? `${br(e.sd.livreMB / 1024, 1)} GB livres de ${br(e.sd.totalMB / 1024, 1)} GB` : 'ausente') +
    par('Wi-Fi', e.rede.ip ? `${e.rede.ssid} · ${e.rede.ip} · ${e.rede.rssi} dBm` : 'desconectado') +
    par('Modo AP', e.rede.ap ? 'ligado' : 'desligado') +
    par('Ligada há', `${horas} h ${min} min`) +
    par('Memória livre', `${e.sis.ram} KB interna · ${br(e.sis.psram / 1024, 1)} MB PSRAM`);
}

// ---------------------------------------------------------------- grafico
function desenhaGrafico() {
  const cv = $('grafico'), dpr = window.devicePixelRatio || 1;
  const W = cv.clientWidth, H = cv.clientHeight;
  if (cv.width !== W * dpr) { cv.width = W * dpr; cv.height = H * dpr; }
  const g = cv.getContext('2d');
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, W, H);
  const dados = S.serie;
  let topo = 60;
  for (const p of dados) topo = Math.max(topo, p[0], p[1], p[2], p[3]);
  topo = Math.ceil((topo + 10) / 50) * 50;
  const esq = 34, dir = W - 6, alto = 8, baixo = H - 18;
  const y = (v) => baixo - (v / topo) * (baixo - alto);
  const x = (i) => esq + (i / 599) * (dir - esq);
  g.font = '11px Segoe UI, sans-serif';
  g.fillStyle = '#9aa4ae'; g.strokeStyle = '#2b3239'; g.lineWidth = 1;
  for (let v = 0; v <= topo; v += 50) {
    g.beginPath(); g.moveTo(esq, y(v)); g.lineTo(dir, y(v)); g.stroke();
    g.fillText(v, 4, y(v) + 4);
  }
  g.fillText('−10 min', esq, H - 3);
  g.fillText('agora', dir - 34, H - 3);
  const desloc = 600 - dados.length;
  const linha = (k, cor, tracejado) => {
    g.beginPath(); g.strokeStyle = cor; g.lineWidth = tracejado ? 1.2 : 2;
    g.setLineDash(tracejado ? [5, 4] : []);
    dados.forEach((p, i) => { const px = x(i + desloc), py = y(p[k]); i ? g.lineTo(px, py) : g.moveTo(px, py); });
    g.stroke(); g.setLineDash([]);
  };
  linha(1, '#ff7a45', true); linha(3, '#4aa3ff', true);
  linha(0, '#ff7a45', false); linha(2, '#4aa3ff', false);
}

// ---------------------------------------------------------------- console
function linhaConsole(m) {
  const log = $('log');
  const fundo = log.scrollTop + log.clientHeight >= log.scrollHeight - 30;
  const d = document.createElement('div');
  d.className = m.k;
  const h = new Date();
  const hora = document.createElement('span');
  hora.className = 'hora';
  hora.textContent = h.toLocaleTimeString('pt-BR');
  d.append(hora, (m.k === 'tx' ? '>> ' : '') + m.s);
  log.append(d);
  while (log.childElementCount > 1500) log.firstElementChild.remove();
  if (fundo) log.scrollTop = log.scrollHeight;
}

// ---------------------------------------------------------------- camera
function foto() {
  pararVivo();
  const img = $('imgCam');
  $('semImagem').textContent = 'Fotografando… (a câmera liga em ~2 s)';
  $('semImagem').style.display = 'flex';
  img.onload = () => { $('semImagem').style.display = 'none'; img.hidden = false; };
  img.onerror = () => { $('semImagem').textContent = 'Falha na foto'; img.hidden = true; };
  img.src = '/foto.jpg?t=' + S.token + '&_=' + Date.now();
  $('dicaCam').textContent = 'A câmera desliga sozinha 20 s depois do último uso, para não esquentar.';
}
function vivo() {
  if (S.vivo) { pararVivo(); return; }
  S.vivo = true;
  $('btnVivo').textContent = 'Parar';
  $('semImagem').textContent = 'Ligando a câmera…';
  $('semImagem').style.display = 'flex';
  const img = $('imgCam');
  img.onload = () => { $('semImagem').style.display = 'none'; img.hidden = false; };
  img.src = `http://${location.hostname}:81/stream?t=${S.token}`;
}
function pararVivo() {
  if (!S.vivo) return;
  S.vivo = false;
  $('btnVivo').textContent = 'Ao vivo';
  $('imgCam').src = '';
}

// ---------------------------------------------------------------- acoes
function jog(eixo) {
  const sinal = eixo[1] === '-' ? -1 : 1, ax = eixo[0];
  const dist = (S.passo * sinal).toString();
  const vel = ax === 'Z' ? 600 : 3000;
  envia(`G91\nG1 ${ax}${dist} F${vel}\nG90`);
}

function liga() {
  $('abas').addEventListener('click', (ev) => {
    const b = ev.target.closest('button[data-aba]');
    if (!b) return;
    document.querySelectorAll('#abas button').forEach((x) => x.classList.toggle('ativa', x === b));
    document.querySelectorAll('.aba').forEach((s) => s.classList.toggle('ativa', s.id === b.dataset.aba));
    if (b.dataset.aba !== 'painel') pararVivo();
    if (b.dataset.aba === 'painel') desenhaGrafico();
    if (b.dataset.aba === 'nivel' && window.Nivel) Nivel.mostra();
    if (b.dataset.aba === 'arquivos' && window.Arquivos) Arquivos.atualiza();
    history.replaceState(null, '', '#/' + b.dataset.aba);   // '#/' evita rolar ate a secao
  });
  const inicial = location.hash.replace(/^#\/?/, '');
  if (inicial) document.querySelector(`#abas button[data-aba="${inicial}"]`)?.click();

  document.body.addEventListener('click', (ev) => {
    const b = ev.target.closest('button');
    if (!b) return;
    if (b.dataset.gcode) {
      if (b.dataset.confirma && !confirm(b.dataset.confirma)) return;
      if (envia(b.dataset.gcode)) aviso('Enviado: ' + b.dataset.gcode.split('\n')[0]);
    } else if (b.dataset.jog) {
      jog(b.dataset.jog);
    } else if (b.dataset.passo) {
      S.passo = parseFloat(b.dataset.passo);
      document.querySelectorAll('#passos button').forEach((x) => x.classList.toggle('ativo', x === b));
    } else if (b.dataset.temp) {
      const v = parseInt($(b.dataset.temp === 'bico' ? 'inBico' : 'inMesa').value, 10);
      if (isNaN(v)) return aviso('Digite a temperatura', true);
      ajustaTemp(b.dataset.temp, v);
    } else if (b.dataset.tempPreset) {
      const [qual, v] = b.dataset.tempPreset.split(':');
      ajustaTemp(qual, parseInt(v, 10));
    } else if (b.dataset.extrude) {
      const mm = parseFloat($('inFilMm').value) * parseInt(b.dataset.extrude, 10);
      const vel = parseFloat($('inFilVel').value) * 60;
      if (S.estado && S.estado.tb.bico < 170) return aviso('Bico frio: aqueça acima de 170 °C', true);
      envia(`M83\nG1 E${mm} F${vel}`);
    }
  });

  $('btnParada').onclick = () => {
    if (!confirm('PARADA DE EMERGÊNCIA (M112)?\n\nO Marlin desliga tudo na hora e a TinyBee precisa ser reiniciada.')) return;
    envia('!m112');
  };
  $('btnFoto').onclick = foto;
  $('btnVivo').onclick = vivo;
  $('btnVel').onclick = () => envia('M220 S' + parseInt($('inVel').value, 10));
  $('btnFluxo').onclick = () => envia('M221 S' + parseInt($('inFluxo').value, 10));
  $('btnLimpaTela').onclick = () => { $('log').textContent = ''; };
  $('btnLimpaFila').onclick = () => envia('!limpa');
  $('chkOk').onchange = () => $('log').classList.toggle('semOk', !$('chkOk').checked);
  $('log').classList.add('semOk');

  $('formEnvio').onsubmit = (ev) => {
    ev.preventDefault();
    const v = $('inGcode').value.trim();
    if (!v) return;
    if (/^M112\b/i.test(v)) { $('btnParada').click(); return; }
    if (envia(v)) {
      S.hist.push(v);
      S.histPos = S.hist.length;
      $('inGcode').value = '';
    }
  };
  $('inGcode').addEventListener('keydown', (ev) => {
    if (ev.key === 'ArrowUp' && S.histPos > 0) { $('inGcode').value = S.hist[--S.histPos]; ev.preventDefault(); }
    if (ev.key === 'ArrowDown') {
      S.histPos = Math.min(S.hist.length, S.histPos + 1);
      $('inGcode').value = S.hist[S.histPos] || '';
      ev.preventDefault();
    }
  });
  window.addEventListener('resize', () => desenhaGrafico());
}

function ajustaTemp(qual, v) {
  const max = qual === 'bico' ? 280 : 110;
  if (v < 0 || v > max) return aviso(`Fora da faixa (0 a ${max} °C)`, true);
  if (envia((qual === 'bico' ? 'M104 S' : 'M140 S') + v))
    aviso(v ? `${qual === 'bico' ? 'Bico' : 'Mesa'}: ${v} °C` : `${qual === 'bico' ? 'Bico' : 'Mesa'} desligado`);
}

// ---------------------------------------------------------------- inicio
window.avisoGlobal = aviso;
liga();
sessao().then(conecta).catch(() => {
  aviso('Não consegui abrir a sessão. Recarregue a página.', true);
  setTimeout(() => location.reload(), 5000);
});
