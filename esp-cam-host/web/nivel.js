// Tela de nivelamento: mapa da mesa, molas e diagnostico.
// Porte de tools-printer/nivelamento.py (mesmas regras e o mesmo arquivo de dados).
'use strict';

const Nivel = (() => {
  // ------------------------------------------------------------ geometria
  const MESA_X = 196, MESA_Y = 181, Y_HOME = -15;
  const SONDA = { L: 15, R: 142, F: 15, B: 166 };
  const GRADE_FIRMWARE = 5, ALCANCE_BUSCA = 3.6;
  const MM_VOLTA_PADRAO = 0.85;
  const MAX_HIST = 60;
  const NOMES = { FE: 'Frente esquerda', FD: 'Frente direita', TE: 'Fundo esquerda', TD: 'Fundo direita' };
  const MOLAS_PADRAO = { FE: [0, 0], FD: [MESA_X, 0], TE: [0, MESA_Y], TD: [MESA_X, MESA_Y] };
  const FAIXAS = [
    [0.05, '#1f9d55', 'excelente', 'até 0,05'], [0.10, '#7bc043', 'bom', '0,05 – 0,10'],
    [0.20, '#f2c12e', 'aceitável', '0,10 – 0,20'], [0.30, '#f08a24', 'ruim', '0,20 – 0,30'],
    [Infinity, '#e03b3b', 'crítico', 'acima de 0,30'],
  ];
  const C = { painel: '#1f242a', painel2: '#272d34', borda: '#3a424c', texto: '#e6e9ec',
              suave: '#9aa4ae', vidro: '#2a3139', acento: '#4aa3ff' };

  const $ = (id) => document.getElementById(id);
  const br = (v, n = 2, sinal = false) =>
    (sinal && v >= 0 ? '+' : '') + Number(v).toFixed(n).replace('.', ',');

  // ------------------------------------------------------------ numerica
  function resolve(M, v) {
    const n = v.length, A = M.map((l, i) => [...l, v[i]]);
    for (let i = 0; i < n; i++) {
      let p = i;
      for (let k = i + 1; k < n; k++) if (Math.abs(A[k][i]) > Math.abs(A[p][i])) p = k;
      if (Math.abs(A[p][i]) < 1e-12) throw new Error('singular');
      [A[i], A[p]] = [A[p], A[i]];
      for (let k = 0; k < n; k++) if (k !== i) {
        const f = A[k][i] / A[i][i];
        for (let j = 0; j <= n; j++) A[k][j] -= f * A[i][j];
      }
    }
    return A.map((l, i) => l[n] / l[i]);
  }
  function plano(pts) {               // z = a + b x + c y
    const S = [[0, 0, 0], [0, 0, 0], [0, 0, 0]], v = [0, 0, 0];
    for (const [x, y, z] of pts) {
      const r = [1, x, y];
      for (let i = 0; i < 3; i++) { v[i] += r[i] * z; for (let j = 0; j < 3; j++) S[i][j] += r[i] * r[j]; }
    }
    return resolve(S, v);
  }
  const mediana = (a) => {
    if (!a.length) return 0;
    const s = [...a].sort((p, q) => p - q), n = s.length;
    return n % 2 ? s[n >> 1] : (s[n / 2 - 1] + s[n / 2]) / 2;
  };
  const linspace = (a, b, n) => n === 1 ? [a] : Array.from({ length: n }, (_, i) => a + (b - a) * i / (n - 1));
  function faixa(d) {
    const a = Math.abs(d);
    for (const f of FAIXAS) if (a <= f[0]) return f;
    return FAIXAS[FAIXAS.length - 1];
  }
  const hex = (c) => [1, 3, 5].map((i) => parseInt(c.slice(i, i + 2), 16));
  function mistura(c1, c2, t) {
    const a = hex(c1), b = hex(c2);
    return '#' + a.map((v, i) => Math.round(v + (b[i] - v) * t).toString(16).padStart(2, '0')).join('');
  }
  function corContinua(d) {
    const a = Math.abs(d);
    const pts = [[0, FAIXAS[0][1]], ...FAIXAS.slice(0, -1).map((f, i) => [f[0], FAIXAS[i + 1][1]]),
                 [0.45, FAIXAS[4][1]]];
    for (let i = 1; i < pts.length; i++)
      if (a <= pts[i][0]) return mistura(pts[i - 1][1], pts[i][1], (a - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]));
    return FAIXAS[4][1];
  }
  function textoVoltas(v) {
    const oit = Math.round(Math.abs(v) * 8), inteiro = Math.floor(oit / 8), resto = oit % 8;
    const frac = ['', '⅛', '¼', '⅜', '½', '⅝', '¾', '⅞'][resto];
    if (!oit) return 'menos de ⅛ de volta';
    const t = inteiro && frac ? `${inteiro} ${frac}` : inteiro ? `${inteiro}` : frac;
    return t + (oit <= 8 ? ' volta' : ' voltas');
  }

  // ------------------------------------------------------------ analise
  const mesmosPontos = (a, b) => a && b && a.xs.length === b.xs.length && a.ys.length === b.ys.length &&
    [...a.xs, ...a.ys].every((v, i) => Math.abs(v - [...b.xs, ...b.ys][i]) < 0.6);
  const validos = (m) => m.z.flatMap((l, j) => l.map((z, i) => [m.xs[i], m.ys[j], z])).filter((p) => p[2] != null);

  function correcaoMolas(pl, molas, mmVolta, ref) {
    const z = {};
    for (const k in molas) z[k] = pl[0] + pl[1] * molas[k][0] + pl[2] * molas[k][1];
    const ks = Object.keys(z);
    let r;
    if (ref === 'alto') r = ks.reduce((a, b) => z[a] >= z[b] ? a : b);
    else if (ref === 'baixo') r = ks.reduce((a, b) => z[a] <= z[b] ? a : b);
    else r = ks.reduce((a, b) => ks.reduce((s, o) => s + Math.abs(z[a] - z[o]), 0) <=
                                  ks.reduce((s, o) => s + Math.abs(z[b] - z[o]), 0) ? a : b);
    const out = {};
    for (const k of ks) { const mm = z[r] - z[k]; out[k] = { mm, voltas: mm / mmVolta, ref: k === r }; }
    return out;
  }

  function calibra(m0, m1, giros, molas) {
    if (!mesmosPontos(m0, m1)) throw new Error('as duas medições precisam ter os mesmos pontos');
    if (!Object.values(giros).some((v) => Math.abs(v) > 1e-9)) throw new Error('informe as voltas dadas');
    const p0 = plano(validos(m0)), p1 = plano(validos(m1));
    const obs = [p1[1] - p0[1], p1[2] - p0[2]];
    const pr = plano(Object.entries(molas).map(([k, [x, y]]) => [x, y, giros[k] || 0])).slice(1);
    const den = pr[0] ** 2 + pr[1] ** 2;
    if (den < 1e-12) throw new Error('esse giro não inclina a mesa');
    const p = (obs[0] * pr[0] + obs[1] * pr[1]) / den;
    if (p <= 0) throw new Error('a mesa inclinou ao contrário: confira o sinal das voltas');
    return p;
  }

  function analisa(m, cfg, anterior) {
    const { xs, ys, z: Z } = m, nx = xs.length, ny = ys.length;
    const pts = validos(m), vals = pts.map((p) => p[2]);
    const media = vals.reduce((s, v) => s + v, 0) / vals.length;
    const pl = pts.length >= 3 && nx > 1 && ny > 1 ? plano(pts) : [media, 0, 0];
    const resid = Z.map((l, j) => l.map((v, i) => v == null ? null : v - (pl[0] + pl[1] * xs[i] + pl[2] * ys[j])));
    const rv = resid.flat().filter((v) => v != null);

    // pontos que destoam dos vizinhos (no residuo, com 3+ vizinhos)
    const lap = new Map();
    for (let j = 0; j < ny; j++) for (let i = 0; i < nx; i++) {
      if (resid[j][i] == null) continue;
      const viz = [[j - 1, i], [j + 1, i], [j, i - 1], [j, i + 1]]
        .filter(([a, b]) => a >= 0 && a < ny && b >= 0 && b < nx && resid[a][b] != null).map(([a, b]) => resid[a][b]);
      if (viz.length >= 3) lap.set(j + ',' + i, resid[j][i] - viz.reduce((s, v) => s + v, 0) / viz.length);
    }
    const refLap = Math.max(0.2, 3 * mediana([...lap.values()].map(Math.abs)));
    const suspeitos = new Map();
    for (const [k, d] of lap) if (Math.abs(d) > refLap) suspeitos.set(k, `${br(d, 2, true)} mm em relação aos vizinhos`);

    const varTot = rv.reduce((s, r) => s + r * r, 0) || 1e-12;
    const col = xs.map((_, i) => resid.reduce((s, l) => s + (l[i] ?? 0), 0) / ny);
    const lin = resid.map((l) => l.reduce((s, v) => s + (v ?? 0), 0) / nx);
    const fracX = col.reduce((s, v) => s + v * v, 0) * ny / varTot;
    const fracY = lin.reduce((s, v) => s + v * v, 0) * nx / varTot;
    const borda = [], miolo = [];
    resid.forEach((l, j) => l.forEach((v, i) => {
      if (v == null) return;
      (i === 0 || i === nx - 1 || j === 0 || j === ny - 1 ? borda : miolo).push(v);
    }));
    const med = (a) => a.reduce((s, v) => s + v, 0) / a.length;
    const cupula = miolo.length && borda.length ? med(miolo) - med(borda) : 0;

    let comp = null;
    if (anterior && mesmosPontos(m, anterior)) {
      const dif = Z.map((l, j) => l.map((v, i) => v == null || anterior.z[j][i] == null ? null : v - anterior.z[j][i]));
      const dpts = dif.flatMap((l, j) => l.map((d, i) => [xs[i], ys[j], d])).filter((p) => p[2] != null);
      let pd = nx > 1 && ny > 1 ? plano(dpts) : [mediana(dpts.map((p) => p[2])), 0, 0];
      let rel, lim;
      for (let k = 0; k < 2; k++) {       // duas passadas: um ponto ruim nao entorta o plano
        rel = dif.map((l, j) => l.map((d, i) => d == null ? null : d - (pd[0] + pd[1] * xs[i] + pd[2] * ys[j])));
        lim = Math.max(0.08, 3 * 1.4826 * mediana(rel.flat().filter((v) => v != null).map(Math.abs)));
        const bons = dpts.filter(([x, y, d]) => Math.abs(d - (pd[0] + pd[1] * x + pd[2] * y)) <= lim);
        if (bons.length >= 3 && nx > 1 && ny > 1) pd = plano(bons);
      }
      const desloc = pd[0] + pd[1] * med(xs) + pd[2] * med(ys);
      const inclMudou = Math.hypot(pd[1] * MESA_X, pd[2] * MESA_Y);
      const fora = [];
      rel.forEach((l, j) => l.forEach((v, i) => { if (v != null && Math.abs(v) > lim) fora.push(j + ',' + i); }));
      const bons = rel.flat().filter((v, idx) => v != null && !fora.includes(Math.floor(idx / nx) + ',' + idx % nx));
      const sigma = bons.length ? Math.sqrt(bons.reduce((s, v) => s + v * v, 0) / bons.length) : 0;
      const suspAnt = analisa(anterior, cfg).suspeitos;
      const ruimAnt = fora.filter((k) => suspAnt.has(k) && !suspeitos.has(k));
      const ruimAtual = fora.filter((k) => suspeitos.has(k) && !suspAnt.has(k));
      const reais = [...suspeitos.keys()].filter((k) => !fora.includes(k));
      for (const k of reais) suspeitos.delete(k);
      comp = { desloc, rel, sigma, quando: anterior.quando, fora: fora.filter((k) => !ruimAnt.includes(k)),
               ruimAnterior: ruimAnt, ruimAtual, reais, inclMudou };
    }
    return {
      media, min: Math.min(...vals), max: Math.max(...vals), amplitude: Math.max(...vals) - Math.min(...vals),
      plano: pl, inclX: pl[1] * MESA_X, inclY: pl[2] * MESA_Y, resid,
      residAmp: rv.length ? Math.max(...rv) - Math.min(...rv) : 0, fracX, fracY, cupula, suspeitos,
      molas: correcaoMolas(pl, cfg.molas, cfg.mm_volta, cfg.referencia), comp, nx, ny,
    };
  }

  function diagnostico(m, an, cfg, estado) {
    const out = [], amp = an.amplitude, tol = cfg.tolerancia, { xs, ys } = m;
    const nome = (k) => { const [j, i] = k.split(',').map(Number); return `X${xs[i].toFixed(0)} Y${ys[j].toFixed(0)}`; };
    const comp = an.comp;
    if (comp) {
      if (comp.ruimAnterior.length) out.push(['info', 'Leitura ruim na medição anterior',
        `${comp.ruimAnterior.slice(0, 4).map(nome).join(', ')}: o valor mudou, mas agora concorda com os vizinhos e antes não concordava. A leitura errada era a de ${comp.quando}; esta vale.`]);
      if (comp.ruimAtual.length) out.push(['aviso', 'Leitura ruim nesta medição',
        `${comp.ruimAtual.slice(0, 4).map(nome).join(', ')}: o valor mudou em relação a ${comp.quando} e agora destoa dos vizinhos, o que antes não acontecia. Repita antes de gravar.`]);
      const outros = comp.fora.filter((k) => !comp.ruimAtual.includes(k));
      if (outros.length) out.push(['aviso', 'Pontos que não repetiram',
        `Comparado com ${comp.quando}, ${outros.length} ponto(s) mudaram além do normal (${outros.slice(0, 4).map(nome).join(', ')}). Repita a medição: a terceira mostra qual das duas estava certa.`]);
      if (comp.inclMudou > 0.1) out.push(['info', 'A inclinação mudou',
        `Entre as duas medições a mesa inclinou ${br(comp.inclMudou)} mm de um lado a outro (molas mexidas?). A comparação ponto a ponto já desconta isso.`]);
      out.push([comp.sigma <= 0.03 ? 'ok' : comp.sigma <= 0.06 ? 'info' : 'aviso', 'Repetibilidade',
        `Descontada a mudança geral, os pontos variaram ±${br(comp.sigma, 3)} mm em relação a ${comp.quando}.${comp.sigma <= 0.03 ? ' O probe está confiável.' : ''}`]);
      if (Math.abs(comp.desloc) > 0.05) out.push(['info', 'A mesa inteira mudou de altura',
        `Todos os pontos andaram ${br(comp.desloc, 2, true)} mm juntos. Costuma ser temperatura: meça sempre na mesma condição, de preferência com a mesa quente.`]);
    }
    if (an.suspeitos.size) out.push(['aviso', 'Pontos que destoam dos vizinhos',
      [...an.suspeitos].slice(0, 4).map(([k, t]) => `${nome(k)}: ${t}`).join('; ') +
      '. Pode ser leitura ruim do probe ou um calombo/vale de verdade. Repita a medição: a tela compara as duas. Marcados com ⚠.']);
    if (comp && comp.reais.length) out.push(['info', 'Destoam, mas repetiram',
      `${comp.reais.slice(0, 4).map(nome).join(', ')}: diferentes dos vizinhos nas duas medições, então é a forma da mesa. A compensação cuida disso.`]);

    const giroMax = Math.max(...Object.values(an.molas).map((v) => Math.abs(v.mm)));
    if (giroMax > tol) {
      const ladoX = an.inclX > 0 ? 'a direita' : 'a esquerda';
      const ladoY = an.inclY > 0 ? 'o fundo está' : 'a frente está', altoY = an.inclY > 0 ? 'mais alto' : 'mais alta';
      out.push([giroMax > 0.15 ? 'aviso' : 'info', 'Inclinação (as molas corrigem)',
        `De um lado a outro da mesa, ${ladoX} está ${br(Math.abs(an.inclX))} mm mais alta (eixo X) e ${ladoY} ${br(Math.abs(an.inclY))} mm ${altoY} (eixo Y).`]);
    } else out.push(['ok', 'Inclinação', 'A mesa está no plano: as molas não precisam de ajuste.']);

    if (an.residAmp > 0.15) {
      let t = `Descontada a inclinação, sobra ${br(an.residAmp)} mm de diferença que as molas não corrigem. `;
      if (an.fracX > 0.6) t += 'O padrão se repete em todas as linhas e muda só com o X: aponta para o eixo X variando de altura ao longo do curso, ou o vidro ondulado nessa direção. ';
      else if (an.fracY > 0.6) t += 'O padrão se repete em todas as colunas e muda só com o Y: aponta para a guia do Y ou o vidro ondulado nessa direção. ';
      else if (Math.abs(an.cupula) > 0.08) t += `O centro está ${an.cupula > 0 ? 'mais alto' : 'mais baixo'} que as bordas: a placa da mesa está ${an.cupula > 0 ? 'abaulada' : 'côncava'}, comum quando a mesa aquece. `;
      out.push([an.residAmp <= 0.3 ? 'info' : 'aviso', 'Ondulação', t + 'A compensação do firmware (M420 S1) cobre isso na impressão.']);
    }
    if (amp > ALCANCE_BUSCA * 0.6) out.push(['erro', 'Perto do limite do probe',
      `A amplitude (${br(amp)} mm) está perto do alcance de busca do probe (${br(ALCANCE_BUSCA, 1)} mm). Ajuste as molas antes que algum ponto falhe.`]);
    if (m.tipo === 'G29' && xs[xs.length - 1] < MESA_X - 20) out.push(['info', 'Faixa sem medição',
      `O probe só alcança até X ${xs[xs.length - 1].toFixed(0)} e Y ${ys[ys.length - 1].toFixed(0)}. Dali até a borda (direita e fundo) o firmware repete o valor da última linha de pontos.`]);
    if (m.temp_mesa != null && m.temp_mesa < 40) out.push(['info', 'Medição com a mesa fria',
      `A mesa estava a ${m.temp_mesa.toFixed(0)} °C. Para a malha de impressão, meça com a mesa na temperatura de impressão.`]);
    if (estado.compensacao === false) out.push(['aviso', 'Compensação desligada',
      'O firmware disse "Bed Leveling OFF". O G-code do fatiador faz G29 no início; para mover à mão com a malha, use "Ligar malha".']);
    if (m.tipo === 'G30') out.push(['info', 'Medição rápida',
      'Esta medição (G30) serve para ajustar as molas. A malha que vale para imprimir é a do G29 (feita no início de cada impressão).']);

    let passo;
    if (comp && (comp.fora.length || comp.ruimAtual.length)) passo = ['aviso', 'Próximo passo', 'Repita a medição: há pontos que não repetiram.'];
    else if (an.suspeitos.size && !comp) passo = ['aviso', 'Próximo passo', 'Repita a medição para confirmar os pontos que destoam antes de girar as molas.'];
    else if (giroMax > Math.max(tol, 0.10)) passo = ['aviso', 'Próximo passo', 'Gire as molas como indicam os cards dos cantos (comece pela metade se passar de 1 volta), faça Home e meça de novo.'];
    else if (m.tipo === 'G30') passo = ['ok', 'Próximo passo', 'Molas no ponto. A malha G29 é feita no início de cada impressão.'];
    else passo = ['ok', 'Próximo passo', 'Mesa boa para imprimir.'];
    const nota = amp <= 0.15 ? 'excelente' : amp <= 0.3 ? 'boa' : amp <= 0.5 ? 'aceitável com a compensação' : 'desnivelada';
    out.unshift(passo, [amp <= 0.3 ? 'ok' : amp <= 0.5 ? 'info' : 'aviso', `Mesa ${nota}`,
      `Amplitude de ${br(amp)} mm (de ${br(an.min, 3, true)} a ${br(an.max, 3, true)}).`]);
    return out;
  }

  // ------------------------------------------------------------ estado
  const N = {
    cfg: null, hist: [], atual: null, an: null, estado: { compensacao: null }, temp: null,
    leitor: { ativo: false, cols: null, linhas: {} }, pendente: null, ultimaGrade: ['', 0],
    carregado: false, salvando: null,
  };
  const cfgPadrao = () => ({
    area: { ...SONDA }, area_malha: { ...SONDA }, modo: 'G29', grade_rapida: 3,
    mm_volta: MM_VOLTA_PADRAO, sentido_sobe: '?', referencia: 'auto', tolerancia: 0.05,
    molas: JSON.parse(JSON.stringify(MOLAS_PADRAO)), cor_modo: 'media', temp_mesa: 55,
  });

  async function carrega() {
    N.cfg = cfgPadrao();
    try {
      const r = await fetch('/api/nivel', { cache: 'no-store' });
      if (!r.ok) throw new Error((await r.json().catch(() => ({}))).erro || r.status);
      const d = await r.json();
      Object.assign(N.cfg, d.cfg || {});
      N.hist = (d.historico || []).slice(-MAX_HIST);
    } catch (e) {
      status('Histórico indisponível (' + e.message + '): sem cartão SD, nada será guardado.', 'erro');
    }
    N.atual = N.hist[N.hist.length - 1] || null;
    N.carregado = true;
    preencheForm();
    listaHist();
    recalcula();
  }

  function salva() {           // grava no SD, juntando chamadas proximas
    clearTimeout(N.salvando);
    N.salvando = setTimeout(async () => {
      try {
        const r = await fetch('/api/nivel', { method: 'POST',
          body: JSON.stringify({ cfg: N.cfg, historico: N.hist }) });
        if (!r.ok) throw new Error(r.status);
      } catch (e) { status('Não consegui gravar no SD (' + e.message + ')', 'erro'); }
    }, 400);
  }

  function status(t, tipo) {
    const s = $('nvStatus');
    s.textContent = t;
    s.style.color = tipo === 'erro' ? '#ff7b72' : tipo === 'ok' ? '#7ee787' : tipo === 'acento' ? C.acento : C.suave;
  }

  // ------------------------------------------------------------ formulario
  function preencheForm() {
    const c = N.cfg;
    for (const k of 'LRFB') $('nv' + k).value = c.area[k];
    document.querySelector(`input[name=nvModo][value=${c.modo}]`).checked = true;
    document.querySelector(`input[name=nvRef][value=${c.referencia}]`).checked = true;
    $('nvMmVolta').value = c.mm_volta;
    $('nvTol').value = c.tolerancia;
    marca('nvGrade', 'n', String(c.grade_rapida));
    marca('nvSentido', 's', c.sentido_sobe);
    marca('nvCor', 'cor', c.cor_modo);
    $('nvAquecer').textContent = `🔥 Mesa ${c.temp_mesa} °C`;
    $('nvMolasPos').innerHTML = ['TE', 'TD', 'FE', 'FD'].map((k) =>
      `<label>${NOMES[k]}<span><input type="number" data-mola="${k}" data-i="0" value="${c.molas[k][0]}">` +
      `<input type="number" data-mola="${k}" data-i="1" value="${c.molas[k][1]}"></span></label>`).join('');
    $('nvGiros').innerHTML = ['TE', 'TD', 'FE', 'FD'].map((k) =>
      `<label>${NOMES[k]}<input type="number" step="0.25" value="0" data-giro="${k}"></label>`).join('');
    tempoMedicao();
  }
  function marca(id, attr, valor) {
    document.querySelectorAll(`#${id} button`).forEach((b) => b.classList.toggle('ativo', b.dataset[attr] === valor));
  }
  function leForm() {
    const c = N.cfg, lim = (v, a, b) => Math.max(a, Math.min(b, v));
    c.area = { L: lim(+$('nvL').value, SONDA.L, SONDA.R), R: lim(+$('nvR').value, SONDA.L, SONDA.R),
               F: lim(+$('nvF').value, SONDA.F, SONDA.B), B: lim(+$('nvB').value, SONDA.F, SONDA.B) };
    c.modo = document.querySelector('input[name=nvModo]:checked').value;
    c.referencia = document.querySelector('input[name=nvRef]:checked').value;
    c.mm_volta = Math.max(0.01, +$('nvMmVolta').value || MM_VOLTA_PADRAO);
    c.tolerancia = Math.max(0.01, +$('nvTol').value || 0.05);
    document.querySelectorAll('#nvMolasPos input').forEach((i) => { c.molas[i.dataset.mola][+i.dataset.i] = +i.value; });
    salva();
    tempoMedicao();
    recalcula();
  }
  function tempoMedicao() {
    const a = N.cfg.area;
    $('nvTempo').textContent = N.cfg.modo === 'G29'
      ? `Medir = G28 + G29 L${a.L} R${a.R} F${a.F} B${a.B}: 25 pontos, uns 4 minutos.`
      : `Medir = G28 + ${N.cfg.grade_rapida ** 2} × G30 em X ${a.L}–${a.R}, Y ${a.F}–${a.B}: uns ${Math.ceil(N.cfg.grade_rapida ** 2 * 12 / 60)} min.`;
  }

  // ------------------------------------------------------------ historico
  function nova(tipo, xs, ys, z, nota = '') {
    const d = new Date(), p = (n) => String(n).padStart(2, '0');
    return { quando: `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`,
             tipo, xs: xs.map((v) => Math.round(v * 100) / 100), ys: ys.map((v) => Math.round(v * 100) / 100),
             z, temp_mesa: N.temp, nota };
  }
  function registra(m) {
    N.hist.push(m);
    N.hist = N.hist.slice(-MAX_HIST);
    N.atual = m;
    salva();
    listaHist();
    recalcula();
  }
  function listaHist() {
    const sel = $('nvHist');
    sel.innerHTML = '';
    [...N.hist].reverse().forEach((m, idx) => {
      const v = m.z.flat().filter((x) => x != null);
      const o = document.createElement('option');
      o.value = N.hist.length - 1 - idx;
      o.textContent = `${m.quando.slice(5, 16)}  ${m.tipo} ${m.xs.length}×${m.ys.length}  ${br(Math.max(...v) - Math.min(...v))} mm${m.nota ? '  ' + m.nota : ''}`;
      if (m === N.atual) o.selected = true;
      sel.append(o);
    });
  }
  function anteriorDe(m) {
    const i = N.hist.indexOf(m);
    for (let k = (i < 0 ? N.hist.length : i) - 1; k >= 0; k--) if (mesmosPontos(N.hist[k], m)) return N.hist[k];
    return null;
  }

  // ------------------------------------------------------------ leitura do console
  const RE_G30 = /Bed X:\s*(-?[\d.]+)\s+Y:\s*(-?[\d.]+)\s+Z:\s*(-?[\d.]+)/;
  const RE_FALHA = /probing failed|probe past bed|z probe out|home xyz first|out of bounds|malha falhou/i;

  function linha(m) {
    const l = m.s, L = N.leitor;
    if (/bed leveling on/i.test(l)) N.estado.compensacao = true;
    if (/bed leveling off/i.test(l)) N.estado.compensacao = false;
    if (N.pendente && RE_FALHA.test(l)) {
      if (N.pendente.tipo === 'G30' && !/home/i.test(l)) pontoG30(null);
      else { N.pendente = null; status('Falhou: ' + l.slice(0, 80), 'erro'); }
      return;
    }
    const g = RE_G30.exec(l);
    if (g) { if (N.pendente && N.pendente.tipo === 'G30') pontoG30(parseFloat(g[3])); return; }
    if (/Bilinear Leveling Grid/i.test(l)) { L.ativo = true; L.cols = null; L.linhas = {}; return; }
    if (!L.ativo) return;
    if (L.cols == null) { if (/^\s*0(\s+\d+)+\s*$/.test(l)) L.cols = l.trim().split(/\s+/).length; return; }
    const r = /^\s*(\d+)((?:\s+(?:[+-]?\d+\.\d+|=+))+)\s*$/.exec(l);
    if (r) {
      const t = r[2].trim().split(/\s+/);
      if (t.length === L.cols) { L.linhas[+r[1]] = t.map((v) => v.startsWith('=') ? null : parseFloat(v)); return; }
    }
    L.ativo = false;
    const n = Object.keys(L.linhas).length;
    if (n && Object.keys(L.linhas).every((k, i) => +k === i)) gradeRecebida([...Array(n).keys()].map((j) => L.linhas[j]));
  }

  function gradeRecebida(grade, nota = '') {
    const chave = JSON.stringify(grade);
    if (!nota && N.ultimaGrade[0] === chave && Date.now() - N.ultimaGrade[1] < 120000) return;
    N.ultimaGrade = [chave, Date.now()];
    const a = N.cfg.area_malha || N.cfg.area;
    const m = nova('G29', linspace(a.L, a.R, grade[0].length), linspace(a.F, a.B, grade.length), grade, nota);
    N.pendente = null;
    if (N.atual && JSON.stringify(N.atual.z) === chave && mesmosPontos(N.atual, m) && !nota) {
      status('Malha lida da placa (igual à última guardada)', 'ok');
      return;
    }
    registra(m);
    status('Malha recebida às ' + m.quando.slice(11, 16), 'ok');
  }

  function pontoG30(z) {
    const p = N.pendente, [j, i] = p.ordem[p.idx];
    p.grade[j][i] = z;
    p.idx++;
    status(`Medindo (G30) ${p.idx}/${p.total}…`, 'acento');
    if (p.idx < p.total) return;
    N.pendente = null;
    if (p.grade.flat().every((v) => v == null)) { status('Medição rápida falhou em todos os pontos', 'erro'); return; }
    registra(nova('G30', p.xs, p.ys, p.grade));
    status('Medição rápida concluída', 'ok');
    envia('M420 S1');             // o G30 desliga a compensacao
  }

  // ------------------------------------------------------------ acoes
  function medir() {
    leForm();
    const a = N.cfg.area;
    if (a.R - a.L < 20 || a.B - a.F < 20) return alert('A área precisa ter pelo menos 20 mm em X e em Y.');
    if (N.cfg.modo === 'G29') {
      if (!confirm(`Sondar 25 pontos (~4 min) na área X ${a.L}–${a.R}, Y ${a.F}–${a.B}?\n\nFaz Home antes. Fique perto: no primeiro ponto, confira que o probe não toca nas presilhas do vidro.\n\nDepois, se quiser guardar a malha na EEPROM: Gravar M500.`)) return;
      N.cfg.area_malha = { ...a };
      salva();
      N.pendente = { tipo: 'G29', inicio: Date.now(), total: 25, seg: 10 };
      envia(`G28\nG29 L${a.L} R${a.R} F${a.F} B${a.B}\nM420 V`);
    } else {
      const n = N.cfg.grade_rapida, xs = linspace(a.L, a.R, n), ys = linspace(a.F, a.B, n);
      if (!confirm(`Sondar ${n}×${n} pontos com G30 (~${Math.ceil(n * n * 12 / 60)} min)?\n\nFaz Home antes. Não altera a malha guardada: serve para ajustar as molas.`)) return;
      const ordem = [];
      for (let j = 0; j < n; j++) for (let k = 0; k < n; k++) ordem.push([j, j % 2 ? n - 1 - k : k]);
      N.pendente = { tipo: 'G30', inicio: Date.now(), total: n * n, seg: 12, xs, ys, ordem, idx: 0,
                     grade: Array.from({ length: n }, () => Array(n).fill(null)) };
      envia(['G28', ...ordem.map(([j, i]) => `G30 X${xs[i].toFixed(1)} Y${ys[j].toFixed(1)}`)].join('\n'));
    }
    status('Medindo…', 'acento');
  }

  function calibrar() {
    if (!N.atual) return;
    const ant = anteriorDe(N.atual);
    if (!ant) return alert('Preciso de duas medições com os mesmos pontos.');
    const giros = {};
    document.querySelectorAll('#nvGiros input').forEach((i) => { giros[i.dataset.giro] = +i.value; });
    try {
      const p = calibra(ant, N.atual, giros, N.cfg.molas);
      if (confirm(`Entre ${ant.quando} e ${N.atual.quando}, o rendimento medido foi ${br(p)} mm por volta (atual: ${br(N.cfg.mm_volta)}).\n\nUsar este valor?`)) {
        $('nvMmVolta').value = p.toFixed(2);
        leForm();
      }
    } catch (e) { alert(e.message); }
  }

  function importa(arq) {
    const r = new FileReader();
    r.onload = () => {
      try {
        const d = JSON.parse(r.result);
        const novos = (d.historico || []).filter((m) => m && m.z && !N.hist.some((h) => h.quando === m.quando));
        if (!confirm(`Importar ${novos.length} medição(ões)${d.cfg ? ' e a configuração das molas' : ''}?`)) return;
        if (d.cfg) {
          for (const k of ['mm_volta', 'sentido_sobe', 'referencia', 'tolerancia', 'molas', 'area', 'area_malha'])
            if (d.cfg[k] != null) N.cfg[k] = d.cfg[k];
        }
        N.hist = [...N.hist, ...novos].sort((a, b) => a.quando.localeCompare(b.quando)).slice(-MAX_HIST);
        N.atual = N.hist[N.hist.length - 1] || null;
        salva(); preencheForm(); listaHist(); recalcula();
        status(`${novos.length} medição(ões) importada(s)`, 'ok');
      } catch (e) { alert('Arquivo inválido: ' + e.message); }
    };
    r.readAsText(arq);
  }

  // ------------------------------------------------------------ desenho
  function recalcula() {
    N.an = N.atual ? analisa(N.atual, N.cfg, anteriorDe(N.atual)) : null;
    escreveDiag();
    desenha();
  }

  function escreveDiag() {
    const d = $('nvDiag');
    if (!N.atual) {
      d.innerHTML = '<div>Sem medição. Faça Home e clique em Medir, ou em "Ler malha" para ver a malha gravada na placa.</div>';
      $('nvResumo').textContent = '';
      return;
    }
    const ic = { ok: '✔', info: '●', aviso: '▲', erro: '✖' };
    const esc = (t) => t.replace(/[&<>]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c]));
    d.innerHTML = diagnostico(N.atual, N.an, N.cfg, N.estado)
      .map(([n, t, x]) => `<div><b class="${n}">${ic[n]} ${esc(t)}:</b> ${esc(x)}</div>`).join('');
    const an = N.an, m = N.atual;
    $('nvResumo').textContent = `${m.tipo} ${an.nx}×${an.ny} · ${m.quando.slice(0, 16)} · amplitude ${br(an.amplitude)} mm · inclinação X ${br(Math.abs(an.inclX))} / Y ${br(Math.abs(an.inclY))} · ondulação ${br(an.residAmp)}`;
  }

  function grade() {
    const m = N.atual, an = N.an, modo = N.cfg.cor_modo;
    if (modo === 'anterior' && an.comp) return [an.comp.rel, 'Mudança desde a medição anterior (mm)'];
    if (modo === 'plano') return [an.resid, 'O que sobra depois das molas (mm)'];
    return [m.z.map((l) => l.map((v) => v == null ? null : v - an.media)), 'Desvio da média (mm)'];
  }

  function rr(g, x0, y0, x1, y1, r) {
    r = Math.min(r, (x1 - x0) / 2, (y1 - y0) / 2);
    g.beginPath(); g.moveTo(x0 + r, y0);
    g.arcTo(x1, y0, x1, y1, r); g.arcTo(x1, y1, x0, y1, r); g.arcTo(x0, y1, x0, y0, r); g.arcTo(x0, y0, x1, y0, r);
    g.closePath();
  }

  function desenha() {
    const cv = $('nvCanvas');
    if (!cv.clientWidth) return;
    const dpr = window.devicePixelRatio || 1, W = cv.clientWidth, H = cv.clientHeight;
    if (cv.width !== Math.round(W * dpr) || cv.height !== Math.round(H * dpr)) { cv.width = W * dpr; cv.height = H * dpr; }
    const g = cv.getContext('2d');
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.clearRect(0, 0, W, H);
    const estreito = W < 620;
    const padX = estreito ? 12 : 168, padT = 30, padB = estreito ? 210 : 74;
    const s = Math.min((W - 2 * padX) / MESA_X, (H - padT - padB) / (MESA_Y - Y_HOME));
    const ox = (W - MESA_X * s) / 2, oy = padT + (MESA_Y - Y_HOME) * s;
    const T = (x, y) => [ox + x * s, oy - (y - Y_HOME) * s];
    const txt = (t, x, y, cor, tam, peso = '', alin = 'center') => {
      g.fillStyle = cor; g.font = `${peso} ${tam}px Segoe UI, sans-serif`; g.textAlign = alin; g.textBaseline = 'middle';
      g.fillText(t, x, y);
    };

    // presilhas, vidro, alcance
    const [x0, y0] = T(0, Y_HOME), [x1, y1] = T(MESA_X, 0);
    g.fillStyle = '#23282e'; g.fillRect(x0, y1, x1 - x0, y0 - y1);
    txt('presilhas do vidro (Y −15 a 0)', (x0 + x1) / 2, (y0 + y1) / 2, C.suave, 11);
    const [gx0, gy0] = T(0, MESA_Y), [gx1, gy1] = T(MESA_X, 0);
    rr(g, gx0, gy0, gx1, gy1, 10); g.fillStyle = C.vidro; g.fill(); g.strokeStyle = '#4b5662'; g.lineWidth = 2; g.stroke();
    const [px0, py0] = T(SONDA.L, SONDA.B), [px1, py1] = T(SONDA.R, SONDA.F);
    g.setLineDash([4, 4]); g.strokeStyle = '#56616d'; g.lineWidth = 1; g.strokeRect(px0, py0, px1 - px0, py1 - py0); g.setLineDash([]);
    txt('FUNDO', (gx0 + gx1) / 2, gy0 - 14, C.suave, 12, 'bold');
    txt('FRENTE  (você está aqui)', (x0 + x1) / 2, y0 + 14, C.suave, 12, 'bold');
    g.save(); g.translate(gx0 - 12, (gy0 + gy1) / 2); g.rotate(-Math.PI / 2); txt('ESQUERDA', 0, 0, C.suave, 12, 'bold'); g.restore();
    g.save(); g.translate(gx1 + 12, (gy0 + gy1) / 2); g.rotate(Math.PI / 2); txt('DIREITA', 0, 0, C.suave, 12, 'bold'); g.restore();

    const [titulo] = N.atual ? [grade()[1]] : ['Desvio da média (mm)'];
    legenda(g, W, H, titulo, estreito);
    if (!N.atual) {
      txt('Sem medição', (gx0 + gx1) / 2, (gy0 + gy1) / 2 - 12, C.texto, 16, 'bold');
      txt('Faça Home e clique em Medir, ou em "Ler malha"', (gx0 + gx1) / 2, (gy0 + gy1) / 2 + 12, C.suave, 13);
      molas(g, T, null, W, estreito);
      return;
    }
    const m = N.atual, an = N.an, [gr] = grade(), { xs, ys } = m, nx = xs.length, ny = ys.length;
    const [rx] = T(SONDA.R, 0);

    // mapa de calor interpolado
    if (nx > 1 && ny > 1) {
      const P = 36;
      for (let a = 0; a < P; a++) for (let b = 0; b < P; b++) {
        const fx = (a + 0.5) / P * (nx - 1), fy = (b + 0.5) / P * (ny - 1);
        const i0 = Math.min(Math.floor(fx), nx - 2), j0 = Math.min(Math.floor(fy), ny - 2), tx = fx - i0, ty = fy - j0;
        const c = [gr[j0][i0], gr[j0][i0 + 1], gr[j0 + 1][i0], gr[j0 + 1][i0 + 1]];
        if (c.some((v) => v == null)) continue;
        const v = (c[0] * (1 - tx) + c[1] * tx) * (1 - ty) + (c[2] * (1 - tx) + c[3] * tx) * ty;
        const [xa, ya] = T(xs[0] + (xs[nx - 1] - xs[0]) * a / P, ys[0] + (ys[ny - 1] - ys[0]) * (b + 1) / P);
        const [xb, yb] = T(xs[0] + (xs[nx - 1] - xs[0]) * (a + 1) / P, ys[0] + (ys[ny - 1] - ys[0]) * b / P);
        g.fillStyle = mistura(corContinua(v), C.vidro, 0.55);
        g.fillRect(xa, ya, xb - xa + 1, yb - ya + 1);
      }
    }
    const dx = nx > 1 ? (xs[1] - xs[0]) * s : 80, dy = ny > 1 ? (ys[1] - ys[0]) * s : 60;
    const cw = Math.max(44, Math.min(dx * 0.74, 90)), ch = Math.max(30, Math.min(dy * 0.6, 56));
    g.save(); g.translate((rx + cw / 2 + gx1) / 2, (gy0 + gy1) / 2); g.rotate(Math.PI / 2);
    txt('fora do alcance do probe · o firmware repete a borda', 0, 0, '#6c7782', 10); g.restore();

    N.cards = [];
    for (let j = 0; j < ny; j++) for (let i = 0; i < nx; i++) {
      const [cx, cy] = T(xs[i], ys[j]), v = m.z[j][i], d = gr[j][i];
      const k = j + ',' + i;
      N.cards.push({ x0: cx - cw / 2, y0: cy - ch / 2, x1: cx + cw / 2, y1: cy + ch / 2, j, i });
      if (v == null) {
        rr(g, cx - cw / 2, cy - ch / 2, cx + cw / 2, cy + ch / 2, 8); g.fillStyle = '#444'; g.fill();
        txt('sem leitura', cx, cy, C.texto, 10);
        continue;
      }
      const cor = faixa(d ?? 0)[1], claro = cor === FAIXAS[1][1] || cor === FAIXAS[2][1];
      const fg = claro ? '#15181b' : '#ffffff';
      const susp = an.suspeitos.has(k) || (an.comp && (an.comp.fora.includes(k) || an.comp.ruimAtual.includes(k)));
      rr(g, cx - cw / 2 + 2, cy - ch / 2 + 3, cx + cw / 2 + 2, cy + ch / 2 + 3, 9); g.fillStyle = '#0e1013'; g.fill();
      rr(g, cx - cw / 2, cy - ch / 2, cx + cw / 2, cy + ch / 2, 9); g.fillStyle = cor; g.fill();
      g.lineWidth = susp ? 3 : 1; g.strokeStyle = susp ? '#ffffff' : mistura(cor, '#000000', 0.3); g.stroke();
      const fz = cw >= 62 ? 13 : 11;
      txt(br(v, 3, true), cx, cy - ch * 0.15, fg, fz, 'bold');
      if (ch >= 36) {
        const seta = susp ? '⚠ suspeito' : (d ?? 0) > 0.025 ? '▲ alto' : (d ?? 0) < -0.025 ? '▼ baixo' : '● nível';
        txt(seta, cx, cy + ch * 0.25, fg, 10);
      }
    }
    molas(g, T, an, W, estreito);
  }

  function molas(g, T, an, W, estreito) {
    const tol = N.cfg.tolerancia, sentido = N.cfg.sentido_sobe;
    const [gx0, gy0] = T(0, MESA_Y), [gx1, gy1] = T(MESA_X, Y_HOME);
    const cw = estreito ? (W - 36) / 2 : 146, ch = estreito ? 92 : 104;
    for (const k of ['TE', 'TD', 'FE', 'FD']) {
      const [mx, my] = N.cfg.molas[k], [px, py] = T(mx, my);
      const esq = k.endsWith('E'), frente = k.startsWith('F');
      let cx, cy;
      if (estreito) {
        cx = esq ? 12 + cw / 2 : W - 12 - cw / 2;
        cy = gy1 + 40 + (frente ? ch + 8 : 0) + ch / 2;
      } else {
        cx = esq ? gx0 - 26 - cw / 2 : gx1 + 26 + cw / 2;
        cx = Math.max(cw / 2 + 4, Math.min(W - cw / 2 - 4, cx));
        cy = frente ? gy1 - ch / 2 : gy0 + ch / 2;
      }
      if (!estreito) {
        g.setLineDash([3, 3]); g.strokeStyle = '#56616d'; g.lineWidth = 1;
        g.beginPath(); g.moveTo(px, py); g.lineTo(cx + (esq ? cw / 2 : -cw / 2), cy); g.stroke(); g.setLineDash([]);
      }
      g.beginPath(); g.arc(px, py, 8, 0, 2 * Math.PI); g.fillStyle = '#596673'; g.fill();
      g.strokeStyle = '#c0c8d0'; g.lineWidth = 2; g.stroke();
      g.beginPath(); g.moveTo(px - 4, py); g.lineTo(px + 4, py); g.strokeStyle = '#1a1d21'; g.stroke();

      let cor = C.painel2, tit = '—', l2 = 'sem medição', l3 = '';
      if (an) {
        const c = an.molas[k];
        if (c.ref) { cor = '#2d5b8a'; tit = 'REFERÊNCIA'; l2 = 'não mexer'; l3 = 'as outras se acertam por esta'; }
        else if (Math.abs(c.mm) <= tol) { cor = '#1f9d55'; tit = 'OK'; l2 = 'não mexer'; l3 = `${br(c.mm, 2, true)} mm`; }
        else {
          const sobe = c.mm > 0;
          cor = Math.abs(c.mm) > 0.3 ? '#c26a16' : '#a8861c';
          tit = (sobe ? 'SUBIR ' : 'DESCER ') + textoVoltas(c.voltas);
          l2 = sentido === '?' ? 'sentido: escolha em Molas'
            : ((sentido === 'horario') === sobe ? '↻ sentido horário' : '↺ sentido anti-horário');
          l3 = `${br(c.mm, 2, true)} mm no canto`;
        }
      }
      const x0 = cx - cw / 2, y0 = cy - ch / 2;
      rr(g, x0 + 2, y0 + 3, x0 + cw + 2, y0 + ch + 3, 12); g.fillStyle = '#0e1013'; g.fill();
      rr(g, x0, y0, x0 + cw, y0 + ch, 12); g.fillStyle = C.painel2; g.fill(); g.strokeStyle = cor; g.lineWidth = 2; g.stroke();
      g.save(); rr(g, x0, y0, x0 + cw, y0 + ch, 12); g.clip(); g.fillStyle = cor; g.fillRect(x0, y0, cw, 24); g.restore();
      const t = (s, y, cor2, tam, peso = '') => {
        g.fillStyle = cor2; g.font = `${peso} ${tam}px Segoe UI, sans-serif`; g.textAlign = 'center'; g.textBaseline = 'middle';
        g.fillText(s, cx, y, cw - 10);
      };
      t('Mola · ' + NOMES[k].toLowerCase(), y0 + 12, '#ffffff', 11, 'bold');
      t(tit, y0 + 44, C.texto, tit.length > 16 ? 13 : 15, 'bold');
      t(l2, y0 + 66, C.suave, 11);
      t(l3, y0 + 84, C.suave, 10);
    }
  }

  function legenda(g, W, H, titulo, estreito) {
    const n = FAIXAS.length, lw = Math.min(124, (W - 24) / n), x = (W - lw * n) / 2, y = H - 30;
    g.fillStyle = C.suave; g.font = '11px Segoe UI, sans-serif'; g.textAlign = 'center'; g.textBaseline = 'middle';
    g.fillText(titulo, W / 2, y - 14);
    FAIXAS.forEach(([, cor, nome, f], i) => {
      rr(g, x + i * lw + 3, y - 3, x + (i + 1) * lw - 3, y + 25, 7); g.fillStyle = cor; g.fill();
      const fg = i === 1 || i === 2 ? '#15181b' : '#ffffff';
      g.fillStyle = fg; g.font = 'bold 11px Segoe UI, sans-serif'; g.fillText(nome, x + (i + 0.5) * lw, y + 5);
      g.font = '10px Segoe UI, sans-serif'; g.fillText(f, x + (i + 0.5) * lw, y + 18);
    });
  }

  function dica(ev) {
    const cv = $('nvCanvas'), r = cv.getBoundingClientRect(), x = ev.clientX - r.left, y = ev.clientY - r.top;
    const c = (N.cards || []).find((k) => x >= k.x0 && x <= k.x1 && y >= k.y0 && y <= k.y1);
    if (!c || !N.atual) { cv.title = ''; return; }
    const m = N.atual, an = N.an, v = m.z[c.j][c.i];
    if (v == null) { cv.title = 'sem leitura'; return; }
    const linhas = [`Ponto X ${m.xs[c.i].toFixed(1)}  Y ${m.ys[c.j].toFixed(1)}`, `Medido: ${br(v, 3, true)} mm`,
      `Desvio da média: ${br(v - an.media, 3, true)} mm`, `Depois das molas: ${br(an.resid[c.j][c.i], 3, true)} mm`];
    if (an.comp && an.comp.rel[c.j][c.i] != null) linhas.push(`Mudou desde a anterior: ${br(an.comp.rel[c.j][c.i], 3, true)} mm`);
    const k = c.j + ',' + c.i;
    if (an.suspeitos.has(k)) linhas.push('⚠ ' + an.suspeitos.get(k));
    cv.title = linhas.join('\n');
  }

  // ------------------------------------------------------------ ligacao com a pagina
  // S e o estado de app.js (const global, nao fica em window)
  function envia(t) { if (typeof S !== 'undefined' && S.ws && S.ws.readyState === 1) S.ws.send(t); else alert('Sem conexão com a placa'); }

  function liga() {
    $('nvMedir').onclick = medir;
    $('nvAreaMax').onclick = () => { for (const k of 'LRFB') $('nv' + k).value = SONDA[k]; leForm(); };
    $('nvMmPadrao').onclick = () => { $('nvMmVolta').value = MM_VOLTA_PADRAO; leForm(); };
    $('nvCalibrar').onclick = calibrar;
    $('nvAquecer').onclick = () => envia('M140 S' + N.cfg.temp_mesa);
    ['nvL', 'nvR', 'nvF', 'nvB', 'nvMmVolta', 'nvTol'].forEach((id) => $(id).addEventListener('change', leForm));
    document.querySelectorAll('input[name=nvModo], input[name=nvRef]').forEach((i) => i.addEventListener('change', leForm));
    $('nvMolasPos').addEventListener('change', leForm);
    $('nvGrade').onclick = (ev) => { const b = ev.target.closest('button'); if (!b) return; N.cfg.grade_rapida = +b.dataset.n; marca('nvGrade', 'n', b.dataset.n); leForm(); };
    $('nvSentido').onclick = (ev) => { const b = ev.target.closest('button'); if (!b) return; N.cfg.sentido_sobe = b.dataset.s; marca('nvSentido', 's', b.dataset.s); leForm(); };
    $('nvCor').onclick = (ev) => { const b = ev.target.closest('button'); if (!b) return; N.cfg.cor_modo = b.dataset.cor; marca('nvCor', 'cor', b.dataset.cor); salva(); desenha(); };
    $('nvHist').onchange = () => { N.atual = N.hist[+$('nvHist').value]; recalcula(); };
    $('nvApagar').onclick = () => {
      if (!N.atual || !confirm(`Apagar a medição de ${N.atual.quando}?`)) return;
      N.hist = N.hist.filter((m) => m !== N.atual);
      N.atual = N.hist[N.hist.length - 1] || null;
      salva(); listaHist(); recalcula();
    };
    $('nvAnotar').onclick = () => {
      if (!N.atual) return;
      const t = prompt('Nota para esta medição (ex.: depois de 1 volta no fundo-esq.)', N.atual.nota || '');
      if (t == null) return;
      N.atual.nota = t.slice(0, 60);
      salva(); listaHist();
    };
    $('nvImportar').onclick = () => $('nvArquivo').click();
    $('nvArquivo').onchange = () => { if ($('nvArquivo').files[0]) importa($('nvArquivo').files[0]); $('nvArquivo').value = ''; };
    $('nvCanvas').addEventListener('mousemove', dica);
    window.addEventListener('resize', () => { if (document.querySelector('#nivel.ativa')) desenha(); });
    setInterval(() => {
      const p = N.pendente;
      if (!p) return;
      const dec = (Date.now() - p.inicio) / 1000, falta = Math.max(0, p.total * p.seg - dec);
      if (dec > p.total * p.seg * 2.5 + 60) { N.pendente = null; status('Sem resultado da placa: medição abandonada', 'erro'); }
      else if (p.tipo === 'G29') status(`Medindo (G29)… ${Math.round(dec)} s, ~${Math.round(falta)} s restantes`, 'acento');
    }, 1000);
    carrega();
  }

  return {
    linha,
    estado(e) { N.temp = e.tb.mesa ? Math.round(e.tb.mesa * 10) / 10 : null; },
    mostra() { if (N.carregado) desenha(); },
    liga,
    // para testes
    _analisa: analisa, _diagnostico: diagnostico, _cfgPadrao: cfgPadrao, _textoVoltas: textoVoltas, _calibra: calibra,
  };
})();

// const nao vira propriedade de window: app.js procura window.Nivel
window.Nivel = Nivel;
Nivel.liga();
