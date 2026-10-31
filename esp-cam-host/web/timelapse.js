// Time-lapse: configuracao, lista do que foi gravado, reprodutor e download.
'use strict';

const Timelapse = (() => {
  const $ = (id) => document.getElementById(id);
  const T = { lista: [], cfg: null, atual: null, tocando: null, gravando: false };
  const esc = (t) => String(t).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const foto = (pasta, n) => `/api/timelapse/foto?pasta=${encodeURIComponent(pasta)}&n=${n}`;
  const mb = (b) => (b / 1048576).toFixed(1).replace('.', ',') + ' MB';
  function aviso(t, e) { if (window.avisoGlobal) window.avisoGlobal(t, e); }

  // "20261030_0354_benchy" -> "30/10/2026 03:54 · benchy"
  function rotulo(p) {
    const m = /^(\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})_(.*)$/.exec(p);
    return m ? `${m[3]}/${m[2]}/${m[1]} ${m[4]}:${m[5]} · ${m[6]}` : p;
  }

  async function atualiza() {
    try {
      const r = await fetch('/api/timelapse', { cache: 'no-store' });
      const d = await r.json();
      if (!r.ok) throw new Error(d.erro || r.status);
      T.cfg = d.config;
      T.lista = d.lista.sort((a, b) => b.pasta.localeCompare(a.pasta));
      T.gravando = d.gravando;
      preencheCfg();
      desenhaLista(d);
    } catch (e) {
      $('tlLista').innerHTML = `<p class="dica">Não consegui ler: ${esc(e.message)}</p>`;
    }
  }

  function preencheCfg() {
    const c = T.cfg;
    $('tlAtivo').checked = c.ativo;
    const r = document.querySelector(`input[name=tlModo][value="${c.modo}"]`);
    if (r) r.checked = true;
    $('tlRes').value = String(c.res);
    $('tlFps').value = c.fps;
    $('tlGirar').checked = c.girar !== false;
  }

  function desenhaLista(d) {
    const el = $('tlLista');
    if (!T.lista.length) { el.innerHTML = '<p class="dica">Nenhum time-lapse gravado ainda.</p>'; return; }
    el.innerHTML = T.lista.map((t, i) => {
      const gravandoEste = d.gravando && d.pasta === t.pasta;
      const mini = t.fotos ? `<img loading="lazy" src="${foto(t.pasta, t.fotos)}" alt="">` : '<div class="semMini">sem fotos</div>';
      const det = gravandoEste ? `gravando… ${d.fotos} fotos`
        : `${t.fotos} fotos · ${t.avi ? 'vídeo ' + mb(t.avi) : (d.montando ? 'montando o vídeo…' : 'sem vídeo')}`;
      return `<div class="arqItem">${mini}<div class="info"><div class="nome">${esc(rotulo(t.pasta))}</div>
        <div class="det">${esc(det)}</div></div><div class="acoes">
        ${t.fotos ? `<button data-i="${i}" data-a="ver">Ver</button>` : ''}
        ${t.avi ? `<a href="/api/timelapse/video?pasta=${encodeURIComponent(t.pasta)}"><button class="sec">Baixar</button></a>`
                : (t.fotos > 1 && !gravandoEste ? `<button class="sec" data-i="${i}" data-a="montar">Montar vídeo</button>` : '')}
        ${gravandoEste ? '' : `<button class="sec" data-i="${i}" data-a="apagar">Apagar</button>`}</div></div>`;
    }).join('');
  }

  async function acao(ev) {
    const b = ev.target.closest('button[data-a]');
    if (!b) return;
    const t = T.lista[+b.dataset.i];
    if (b.dataset.a === 'ver') abre(t);
    else if (b.dataset.a === 'apagar') {
      if (!confirm(`Apagar o time-lapse ${rotulo(t.pasta)} (${t.fotos} fotos)?`)) return;
      const r = await fetch('/api/timelapse?pasta=' + encodeURIComponent(t.pasta), { method: 'DELETE' });
      if (!r.ok) aviso((await r.json().catch(() => ({}))).erro || 'Falhou', true);
      atualiza();
    } else if (b.dataset.a === 'montar') {
      await fetch('/api/timelapse/montar?pasta=' + encodeURIComponent(t.pasta), { method: 'POST' });
      aviso('Montando o vídeo; atualize em alguns segundos');
      setTimeout(atualiza, 4000);
    }
  }

  // ------------------------------------------------------------ reprodutor
  function abre(t) {
    para();
    T.atual = t;
    $('tlPlayer').hidden = false;
    $('tlTitulo').textContent = rotulo(t.pasta);
    $('tlFaixa').max = t.fotos;
    $('tlFaixa').value = 1;
    mostra(1);
    $('tlPlayer').scrollIntoView({ behavior: 'smooth', block: 'start' });
  }
  function mostra(n) {
    $('tlImg').src = foto(T.atual.pasta, n);
    $('tlQuadro').textContent = `Foto ${n} de ${T.atual.fotos}`;
  }
  // Reproduz carregando uma foto de cada vez: a velocidade depende do Wi-Fi
  function toca() {
    if (T.tocando) { para(); return; }
    $('tlPlay').textContent = '⏸ Pausar';
    const img = $('tlImg');
    T.tocando = true;
    const proxima = () => {
      if (!T.tocando) return;
      let n = +$('tlFaixa').value + 1;
      if (n > T.atual.fotos) n = 1;
      $('tlFaixa').value = n;
      img.onload = () => setTimeout(proxima, 1000 / Math.max(1, T.cfg ? T.cfg.fps : 10));
      mostra(n);
    };
    proxima();
  }
  function para() {
    T.tocando = null;
    $('tlPlay').textContent = '▶ Reproduzir';
    $('tlImg').onload = null;
  }

  async function salva() {
    const modo = document.querySelector('input[name=tlModo]:checked');
    const q = new URLSearchParams({ ativo: $('tlAtivo').checked ? 1 : 0, modo: modo ? modo.value : 0,
                                    res: $('tlRes').value, fps: $('tlFps').value,
                                    girar: $('tlGirar').checked ? 1 : 0 });
    const r = await fetch('/api/timelapse/config?' + q, { method: 'POST' });
    aviso(r.ok ? 'Configuração salva' : 'Não salvou', !r.ok);
    atualiza();
  }

  function estado(e) {
    const t = e.tl;
    if (!t) return;
    const c = $('tlChip');
    c.className = 'chip ' + (t.gravando ? 'erro' : t.montando ? 'aviso' : '');
    c.textContent = t.gravando ? `● gravando (${t.fotos})` : t.montando ? 'montando vídeo' : 'parado';
    if (T.gravando !== t.gravando && document.querySelector('#timelapse.ativa')) atualiza();
    T.gravando = t.gravando;
  }

  function liga() {
    $('tlLista').addEventListener('click', acao);
    $('tlAtualiza').onclick = atualiza;
    $('tlSalva').onclick = salva;
    $('tlPlay').onclick = toca;
    $('tlFecha').onclick = () => { para(); $('tlPlayer').hidden = true; };
    $('tlFaixa').oninput = () => { para(); mostra(+$('tlFaixa').value); };
    atualiza();      // a aba pode ter sido aberta antes deste script carregar
  }

  return { liga, atualiza, estado };
})();

window.Timelapse = Timelapse;
Timelapse.liga();
