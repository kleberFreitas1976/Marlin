// Arquivos G-code, impressao pela placa e pre-visualizacao por camada.
'use strict';

const Arquivos = (() => {
  const $ = (id) => document.getElementById(id);
  const A = { lista: [], vis: null, ultimoEstado: '' };

  const tamanho = (b) => b > 1048576 ? (b / 1048576).toFixed(1).replace('.', ',') + ' MB'
    : Math.max(1, Math.round(b / 1024)) + ' KB';
  function duracao(s) {
    if (s == null || s < 0) return '–';
    const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60);
    return h ? `${h} h ${String(m).padStart(2, '0')} min` : m ? `${m} min` : `${Math.round(s)} s`;
  }
  const esc = (t) => String(t).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const url = (rota, nome) => `${rota}?nome=${encodeURIComponent(nome)}`;
  function aviso(t, erro) { if (typeof window.avisoGlobal === 'function') window.avisoGlobal(t, erro); else alert(t); }

  // ------------------------------------------------------------ lista
  async function atualiza() {
    try {
      const r = await fetch('/api/arquivos', { cache: 'no-store' });
      const d = await r.json();
      if (!r.ok) throw new Error(d.erro || r.status);
      A.lista = d.sort((a, b) => b.data - a.data);
      desenhaLista();
    } catch (e) {
      $('arqLista').innerHTML = `<p class="dica">Não consegui ler o cartão: ${esc(e.message)}</p>`;
    }
  }

  function desenhaLista() {
    const el = $('arqLista');
    if (!A.lista.length) { el.innerHTML = '<p class="dica">Nenhum G-code no cartão. Envie um ao lado.</p>'; return; }
    el.innerHTML = A.lista.map((a, i) => {
      const m = a.meta || {};
      const det = [tamanho(a.tam), m.tempo > 0 ? '⏱ ' + duracao(m.tempo) : '', m.camadas ? m.camadas + ' camadas' : '',
                   m.filamentoG > 0 ? m.filamentoG.toFixed(1).replace('.', ',') + ' g' : '', m.material || '',
                   m.bico ? `${m.bico}/${m.mesa} °C` : ''].filter(Boolean).join(' · ');
      const mini = m.miniatura ? `<img loading="lazy" src="${url('/api/miniatura', a.nome)}&v=${a.data}" alt="">`
                               : '<div class="semMini">sem miniatura</div>';
      return `<div class="arqItem">${mini}<div class="info"><div class="nome" title="${esc(a.nome)}">${esc(a.nome)}</div>
        <div class="det">${esc(det)}</div><div class="det">${new Date(a.data * 1000).toLocaleString('pt-BR')}</div></div>
        <div class="acoes"><button data-i="${i}" data-acao="imprimir">Imprimir</button>
        <button class="sec" data-i="${i}" data-acao="ver">Ver</button>
        <button class="sec" data-i="${i}" data-acao="apagar">Apagar</button></div></div>`;
    }).join('');
  }

  async function acao(ev) {
    const b = ev.target.closest('button[data-acao]');
    if (!b) return;
    const a = A.lista[+b.dataset.i];
    if (b.dataset.acao === 'apagar') {
      if (!confirm(`Apagar ${a.nome} do cartão?`)) return;
      const r = await fetch(url('/api/arquivos', a.nome), { method: 'DELETE' });
      if (!r.ok) aviso((await r.json().catch(() => ({}))).erro || 'Falhou', true);
      atualiza();
    } else if (b.dataset.acao === 'imprimir') {
      imprimir(a);
    } else if (b.dataset.acao === 'ver') {
      b.disabled = true;
      b.textContent = 'Baixando…';
      try {
        const r = await fetch(url('/api/arquivo', a.nome));
        if (!r.ok) throw new Error(r.status);
        mostra(a.nome, await r.text());
      } catch (e) { aviso('Não consegui ler o arquivo: ' + e.message, true); }
      b.disabled = false;
      b.textContent = 'Ver';
    }
  }

  async function imprimir(a) {
    const m = a.meta || {};
    const e = (typeof S !== 'undefined' && S.estado) ? S.estado : null;
    let txt = `Imprimir ${a.nome}?\n\n`;
    if (m.tempo > 0) txt += `Tempo estimado: ${duracao(m.tempo)} (mais uns 4 min da malha G29 no início)\n`;
    if (m.material) txt += `Material: ${m.material}${m.bico ? ` (${m.bico} °C / mesa ${m.mesa} °C)` : ''}\n`;
    if (e && !e.tb.resp) txt += '\n⚠ A TinyBee não está respondendo.\n';
    txt += '\nConfira: mesa limpa, filamento carregado, nada no caminho.';
    if (!confirm(txt)) return;
    const r = await fetch(url('/api/imprimir', a.nome), { method: 'POST' });
    if (!r.ok) return aviso('Não começou: ' + ((await r.json().catch(() => ({}))).erro || r.status), true);
    aviso('Impressão iniciada');
    document.querySelector('#abas button[data-aba="painel"]').click();
  }

  // ------------------------------------------------------------ envio
  function envia(arq) {
    if (!/\.(gcode|gco|g)$/i.test(arq.name)) return aviso('O arquivo precisa ser .gcode', true);
    if (A.lista.some((a) => a.nome === arq.name) && !confirm(`${arq.name} já existe no cartão. Substituir?`)) return;
    const prog = $('arqProg'), barra = prog.querySelector('i'), txt = prog.querySelector('span');
    prog.hidden = false;
    const x = new XMLHttpRequest();
    const t0 = Date.now();
    x.open('POST', url('/api/arquivos', arq.name));
    x.upload.onprogress = (ev) => {
      const p = ev.loaded / ev.total, vel = ev.loaded / Math.max(0.1, (Date.now() - t0) / 1000);
      barra.style.width = (p * 100).toFixed(1) + '%';
      txt.textContent = `${arq.name}: ${Math.round(p * 100)}% · ${tamanho(vel)}/s`;
    };
    x.onload = () => {
      if (x.status >= 200 && x.status < 300) { txt.textContent = `${arq.name}: enviado`; aviso('Arquivo enviado'); }
      else { txt.textContent = 'Falhou: ' + x.responseText; barra.style.background = 'var(--erro)'; }
      setTimeout(() => { prog.hidden = true; barra.style.width = '0'; barra.style.background = ''; }, 3000);
      atualiza();
    };
    x.onerror = () => { txt.textContent = 'Falhou: conexão interrompida'; };
    x.send(arq);
    // pre-visualiza ja do arquivo local (nao precisa esperar o envio)
    const leitor = new FileReader();
    leitor.onload = () => mostra(arq.name, leitor.result);
    if (arq.size < 60 * 1048576) leitor.readAsText(arq);
  }

  // ------------------------------------------------------------ pre-visualizacao
  const CORES = {
    'External perimeter': '#ff8c3a', 'Outer wall': '#ff8c3a', 'Perimeter': '#ffc04d', 'Inner wall': '#ffc04d',
    'Overhang perimeter': '#4d79ff', 'Overhang wall': '#4d79ff', 'Internal infill': '#b03029', 'Sparse infill': '#b03029',
    'Solid infill': '#9b4dca', 'Internal solid infill': '#9b4dca', 'Top solid infill': '#f05a5a', 'Top surface': '#f05a5a',
    'Bottom surface': '#7a6cff', 'Bridge infill': '#5aa0d6', 'Gap fill': '#ffffff', 'Skirt': '#00a080',
    'Skirt/Brim': '#00a080', 'Brim': '#00a080', 'Support material': '#4fbf3a', 'Support': '#4fbf3a',
    'Support material interface': '#2a8f2a', 'Wipe tower': '#b3b3b3', 'Custom': '#5ad1d6', 'Ironing': '#ff70c0',
    'Purga': '#8fa3b5',
  };
  const corDe = (t) => CORES[t] || '#5ad1d6';

  function analisaGcode(texto) {
    const camadas = [];
    let atual = null, tipo = 'Custom', relXYZ = false, relE = false;
    let x = 0, y = 0, z = 0, e = 0, marcadores = false, zCamada = -1;
    // Com marcas ;LAYER_CHANGE (Orca, Prusa), o que vem antes da primeira (a purga do
    // G-code inicial) entra na primeira camada, em vez de virar uma camada a mais.
    const comMarcas = texto.includes(';LAYER_CHANGE');
    const pre = { z: 0, seg: [], tipos: [] };
    const nova = () => {
      atual = { z, seg: [], tipos: [] };
      if (comMarcas && !camadas.length) { atual.seg.push(...pre.seg); atual.tipos.push(...pre.tipos); }
      camadas.push(atual);
    };
    const linhas = texto.split('\n');
    for (let k = 0; k < linhas.length; k++) {
      const l = linhas[k];
      if (l.charCodeAt(0) === 59) {                      // ';'
        if (l.startsWith(';LAYER_CHANGE')) { marcadores = true; nova(); }
        else if (l.startsWith(';TYPE:')) tipo = l.slice(6).trim();
        else if (l.startsWith(';Z:') && atual) atual.z = parseFloat(l.slice(3));
        continue;
      }
      const c = l.indexOf(';'), g = (c >= 0 ? l.slice(0, c) : l).trim();
      if (!g) continue;
      if (g === 'G90') { relXYZ = false; continue; }
      if (g === 'G91') { relXYZ = true; continue; }
      if (g === 'M82') { relE = false; continue; }
      if (g === 'M83') { relE = true; continue; }
      if (g.startsWith('G92')) { const m = /E(-?[\d.]+)/.exec(g); if (m) e = parseFloat(m[1]); continue; }
      if (!(g.startsWith('G1 ') || g.startsWith('G0 ') || g === 'G1' || g === 'G0')) continue;
      let nx = x, ny = y, nz = z, de = 0;
      const re = /([XYZE])(-?\d*\.?\d+)/g;
      let m;
      while ((m = re.exec(g))) {
        const v = parseFloat(m[2]);
        if (m[1] === 'X') nx = relXYZ ? x + v : v;
        else if (m[1] === 'Y') ny = relXYZ ? y + v : v;
        else if (m[1] === 'Z') nz = relXYZ ? z + v : v;
        else { de = relE ? v : v - e; e = relE ? e + v : v; }
      }
      if (nz !== z) {
        z = nz;
        if (!marcadores && z > zCamada + 1e-4 && de <= 0) { zCamada = z; nova(); }
      }
      if (de > 0 && (nx !== x || ny !== y)) {
        const destino = atual || (comMarcas ? pre : (nova(), atual));
        destino.seg.push(x, y, nx, ny);
        destino.tipos.push(atual ? tipo : 'Purga');
      }
      x = nx; y = ny;
    }
    return camadas.filter((c) => c.seg.length);
  }

  function mostra(nome, texto) {
    const camadas = analisaGcode(texto);
    if (!camadas.length) return aviso('Nenhum movimento de extrusão encontrado no arquivo', true);
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    // tamanho da peca: sem a purga do G-code inicial (o Orca marca como ;TYPE:Custom)
    for (const c of camadas) for (let i = 0, k = 0; i < c.seg.length; i += 4, k++) {
      if (c.tipos[k] === 'Purga' || c.tipos[k] === 'Custom') continue;   // G-code inicial
      for (const j of [i, i + 2]) {
        minX = Math.min(minX, c.seg[j]); maxX = Math.max(maxX, c.seg[j]);
        minY = Math.min(minY, c.seg[j + 1]); maxY = Math.max(maxY, c.seg[j + 1]);
      }
    }
    A.vis = { nome, camadas, ext: [minX, maxX, minY, maxY] };
    $('arqVisual').hidden = false;
    $('visTitulo').textContent = 'Pré-visualização · ' + nome;
    const f = $('visFaixa');
    f.max = camadas.length - 1;
    f.value = Math.min(camadas.length - 1, 0);
    const tipos = [...new Set(camadas.flatMap((c) => c.tipos))];
    $('visLegenda').innerHTML = tipos.map((t) => `<span style="--c:${corDe(t)}">${esc(t)}</span>`).join('');
    const altura = camadas[camadas.length - 1].z;
    $('visInfo').innerHTML = [['Camadas', camadas.length], ['Altura', altura.toFixed(2).replace('.', ',') + ' mm'],
      ['Ocupa em X', `${minX.toFixed(0)} a ${maxX.toFixed(0)} mm`], ['Ocupa em Y', `${minY.toFixed(0)} a ${maxY.toFixed(0)} mm`],
      ['Dentro da mesa', minX >= 0 && maxX <= 196 && minY >= 0 && maxY <= 181 ? 'sim' : '⚠ passa da área útil']]
      .map(([k, v]) => `<div><span>${k}</span><b>${v}</b></div>`).join('');
    desenha();
    $('arqVisual').scrollIntoView({ behavior: 'smooth', block: 'start' });
  }

  function desenha() {
    const V = A.vis;
    if (!V) return;
    const cv = $('visCanvas'), dpr = window.devicePixelRatio || 1, W = cv.clientWidth, H = cv.clientHeight;
    if (!W) return;
    if (cv.width !== Math.round(W * dpr)) { cv.width = W * dpr; cv.height = H * dpr; }
    const g = cv.getContext('2d');
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.clearRect(0, 0, W, H);
    const n = +$('visFaixa').value, c = V.camadas[n];
    $('visCamada').textContent = n + 1;
    $('visTotal').textContent = V.camadas.length;
    $('visZ').textContent = c.z.toFixed(2).replace('.', ',');
    // regiao mostrada: a mesa inteira (196 x 181) ou a peca com 10 mm de folga; frente embaixo
    const [ex0, ex1, ey0, ey1] = $('visMesa').checked ? [0, 196, 0, 181]
      : [V.ext[0] - 10, V.ext[1] + 10, V.ext[2] - 10, V.ext[3] + 10];
    const s = Math.min((W - 30) / (ex1 - ex0), (H - 30) / (ey1 - ey0));
    const ox = (W - (ex1 - ex0) * s) / 2 - ex0 * s, oy = (H + (ey1 - ey0) * s) / 2 + ey0 * s;
    const T = (x, y) => [ox + x * s, oy - y * s];
    g.fillStyle = '#1f252c'; g.fillRect(ox, oy - 181 * s, 196 * s, 181 * s);
    g.strokeStyle = '#3a424c'; g.lineWidth = 1;
    for (let v = 0; v <= 196; v += 20) { const [a] = T(v, 0); g.beginPath(); g.moveTo(a, oy); g.lineTo(a, oy - 181 * s); g.stroke(); }
    for (let v = 0; v <= 181; v += 20) { const [, b] = T(0, v); g.beginPath(); g.moveTo(ox, b); g.lineTo(ox + 196 * s, b); g.stroke(); }
    g.fillStyle = '#9aa4ae'; g.font = '11px Segoe UI'; g.textAlign = 'center';
    g.fillText('frente', ox + 98 * s, oy + 13);
    if (!$('visMesa').checked) {         // regua: tamanho real da regiao mostrada
      g.textAlign = 'left';
      g.fillText(`grade de 20 mm · peça ${(V.ext[1] - V.ext[0]).toFixed(0)} × ${(V.ext[3] - V.ext[2]).toFixed(0)} mm`, 10, H - 8);
    }
    const traco = (cam, cor, larg) => {
      g.lineWidth = larg; g.lineCap = 'round';
      let ultimo = null;
      for (let i = 0, k = 0; i < cam.seg.length; i += 4, k++) {
        const col = cor || corDe(cam.tipos[k]);
        if (col !== ultimo) { if (ultimo) g.stroke(); g.beginPath(); g.strokeStyle = col; ultimo = col; }
        const [a, b] = T(cam.seg[i], cam.seg[i + 1]), [d, e] = T(cam.seg[i + 2], cam.seg[i + 3]);
        g.moveTo(a, b); g.lineTo(d, e);
      }
      if (ultimo) g.stroke();
    };
    if ($('visAnt').checked && n > 0) traco(V.camadas[n - 1], '#3b4550', Math.max(1, 0.4 * s));
    traco(c, null, Math.max(1, 0.42 * s));
  }

  // ------------------------------------------------------------ cartao da impressao
  // "14:32", ou "31/10 14:32" se nao for hoje
  function hora(ms) {
    const d = new Date(ms), h = d.toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit' });
    return d.toDateString() === new Date().toDateString() ? h
      : d.toLocaleDateString('pt-BR', { day: '2-digit', month: '2-digit' }) + ' ' + h;
  }

  function estado(e) {
    const p = e.imp;
    if (!p) return;
    const ativa = ['imprimindo', 'pausando', 'pausada', 'cancelando'].includes(p.estado);
    const recente = ['concluida', 'cancelada', 'erro'].includes(p.estado);
    const c = $('cartaoImp');
    c.hidden = !(ativa || recente || p.tft);
    if (c.hidden) return;
    if (p.tft && !ativa) {                       // impressao da TFT24
      $('impNome').textContent = p.tftNome + ' (pela TFT)';
      $('impEstado').textContent = 'imprimindo';
      $('impEstado').className = 'chip ok';
      const pct = p.tftCamadas ? p.tftCamada / p.tftCamadas : 0;
      $('impBarra').style.width = pct * 100 + '%';
      $('impPct').textContent = p.tftCamadas ? `${Math.round(pct * 100)}% (pelas camadas)` : '';
      $('impCamada').textContent = p.tftCamadas ? `${p.tftCamada} / ${p.tftCamadas}` : '–';
      $('impDecorrido').textContent = duracao(p.tftDecorrido);
      const rest = pct > 0.05 ? p.tftDecorrido * (1 - pct) / pct : -1;
      $('impRestante').textContent = rest >= 0 ? '~' + duracao(rest) : '–';
      $('impTermino').textContent = rest >= 0 ? hora(Date.now() + rest * 1000) : '–';
      $('impAviso').textContent = 'Acompanhada pelas linhas M118 do G-code. Pausar e cancelar ficam na TFT.';
      $('impBotoes').hidden = true;
      $('impMini').hidden = true;
      return;
    }
    $('impBotoes').hidden = !ativa;
    const pct = p.total ? p.bytes / p.total : 0;
    $('impNome').textContent = p.nome;
    const rotulo = { imprimindo: 'imprimindo', pausando: 'pausando…', pausada: 'pausada', cancelando: 'cancelando…',
                     concluida: 'concluída', cancelada: 'cancelada', erro: 'erro' }[p.estado];
    $('impEstado').textContent = rotulo;
    $('impEstado').className = 'chip ' + (p.estado === 'imprimindo' || p.estado === 'concluida' ? 'ok'
      : p.estado === 'erro' || p.estado === 'cancelada' ? 'erro' : 'aviso');
    $('impBarra').style.width = (pct * 100).toFixed(1) + '%';
    $('impPct').textContent = (pct * 100).toFixed(1).replace('.', ',') + '%';
    $('impCamada').textContent = p.camadas ? `${p.camada} / ${p.camadas}` : (p.camada || '–');
    $('impDecorrido').textContent = duracao(p.decorrido);
    $('impRestante').textContent = ativa ? duracao(p.restante) : '–';
    $('impTermino').textContent = ativa && p.restante > 0 ? hora(Date.now() + p.restante * 1000)
      : recente && p.fimHa >= 0 ? hora(Date.now() - p.fimHa * 1000) : '–';
    $('impAviso').textContent = recente && p.fimHa >= 0
      ? `${p.aviso} às ${hora(Date.now() - p.fimHa * 1000)}` : p.aviso;
    $('impPausa').hidden = p.estado !== 'imprimindo';
    $('impRetoma').hidden = p.estado !== 'pausada';
    $('impCancela').hidden = p.estado === 'cancelando';
    const mini = $('impMini');
    const it = A.lista.find((a) => a.nome === p.nome);
    if (it && it.meta && it.meta.miniatura) {
      const src = url('/api/miniatura', p.nome) + '&v=' + it.data;
      if (mini.getAttribute('src') !== src) mini.src = src;
      mini.hidden = false;
    } else mini.hidden = true;
    if (A.ultimoEstado && A.ultimoEstado !== p.estado && recente)
      aviso(p.estado === 'concluida' ? 'Impressão concluída' : p.aviso, p.estado !== 'concluida');
    A.ultimoEstado = p.estado;
  }

  async function controle(acao, pergunta) {
    if (pergunta && !confirm(pergunta)) return;
    const r = await fetch('/api/impressao?acao=' + acao, { method: 'POST' });
    if (!r.ok) aviso('Falhou: ' + ((await r.json().catch(() => ({}))).erro || r.status), true);
  }

  // ------------------------------------------------------------ inicio
  function liga() {
    $('arqLista').addEventListener('click', acao);
    $('arqAtualiza').onclick = atualiza;
    $('arqEscolher').onclick = () => $('arqInput').click();
    $('arqInput').onchange = () => { if ($('arqInput').files[0]) envia($('arqInput').files[0]); $('arqInput').value = ''; };
    const z = $('arqSoltar');
    z.addEventListener('dragover', (ev) => { ev.preventDefault(); z.classList.add('sobre'); });
    z.addEventListener('dragleave', () => z.classList.remove('sobre'));
    z.addEventListener('drop', (ev) => { ev.preventDefault(); z.classList.remove('sobre'); if (ev.dataTransfer.files[0]) envia(ev.dataTransfer.files[0]); });
    $('visFaixa').oninput = desenha;
    $('visAnt').onchange = desenha;
    $('visMesa').onchange = desenha;
    $('visMenos').onclick = () => { $('visFaixa').value = +$('visFaixa').value - 1; desenha(); };
    $('visMais').onclick = () => { $('visFaixa').value = +$('visFaixa').value + 1; desenha(); };
    $('visFecha').onclick = () => { $('arqVisual').hidden = true; A.vis = null; };
    $('impPausa').onclick = () => controle('pausa', 'Pausar? O bico termina os movimentos já enviados, recolhe o filamento e estaciona. O aquecimento continua.');
    $('impRetoma').onclick = () => controle('retoma');
    $('impCancela').onclick = () => controle('cancela', 'CANCELAR a impressão? Bico e mesa desligam e o bico estaciona.');
    $('arqEndereco').textContent = location.origin;
    window.addEventListener('resize', () => { if (A.vis && !$('arqVisual').hidden) desenha(); });
    atualiza();
  }

  return { liga, estado, atualiza, mostra, _analisaGcode: analisaGcode };
})();

window.Arquivos = Arquivos;
Arquivos.liga();
