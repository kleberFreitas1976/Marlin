<?php
// Pagina da impressora na hospedagem: status, ultima foto e comandos dos reles.
declare(strict_types=1);
require __DIR__ . '/comum.php';
sessao();

// ---- foto (so para quem entrou)
if (isset($_GET['foto'])) {
    if (!logado() || !file_exists(caminho('ultima.jpg'))) {
        http_response_code(404);
        exit;
    }
    header('Content-Type: image/jpeg');
    header('Cache-Control: no-store');
    readfile(caminho('ultima.jpg'));
    exit;
}

// ---- entrar / sair
$erro = '';
if (isset($_POST['senha'])) {
    usleep(400000);                       // freia tentativas em sequencia
    if (password_verify($_POST['senha'], $CFG['SENHA_HASH'])) {
        session_regenerate_id(true);
        $_SESSION['ok'] = true;
        header('Location: ./');
        exit;
    }
    $erro = 'Senha incorreta';
}
if (isset($_GET['sair'])) {
    $_SESSION = [];
    session_destroy();
    header('Location: ./');
    exit;
}

// ---- comando (com confirmacao na pagina e token contra CSRF)
$msg = '';
if (logado() && isset($_POST['acao'], $_POST['token']) && hash_equals(token(), $_POST['token'])) {
    $acao = $_POST['acao'];
    if (isset(ACOES[$acao])) {
        $forcar = !empty($_POST['forcar']);
        altera_json('comandos.json', [], function (&$cmds) use ($acao, $forcar) {
            $cmds[] = ['id' => bin2hex(random_bytes(6)), 'acao' => $acao, 'forcar' => $forcar,
                       'pedido' => time()];
        });
        $msg = 'Comando enviado. A placa executa no próximo contato (até ~20 s).';
    }
}

// ---- JSON para a atualizacao automatica da pagina
if (logado() && isset($_GET['json'])) {
    $s = le_json('status.json', null);
    $c = array_slice(array_reverse(le_json('comandos.json', [])), 0, 8);
    responde_json(['status' => $s, 'comandos' => $c, 'agora' => time(),
                   'foto' => file_exists(caminho('ultima.jpg')) ? filemtime(caminho('ultima.jpg')) : 0]);
}

$h = fn($t) => htmlspecialchars((string) $t, ENT_QUOTES, 'UTF-8');
?><!doctype html>
<html lang="pt-br">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="robots" content="noindex">
<title>Impressora</title>
<style>
  :root { --f:#16191d; --p:#1f242a; --p2:#272d34; --b:#3a424c; --t:#e6e9ec; --s:#9aa4ae;
          --a:#4aa3ff; --ok:#1f9d55; --av:#f2c12e; --er:#e03b3b; }
  * { box-sizing:border-box; }
  body { margin:0; background:var(--f); color:var(--t); font:15px "Segoe UI",system-ui,sans-serif; }
  main { max-width:900px; margin:0 auto; padding:16px; }
  .c { background:var(--p); border:1px solid var(--b); border-radius:12px; padding:14px; margin-bottom:14px; }
  h1 { font-size:20px; margin:4px 0 14px; display:flex; justify-content:space-between; align-items:center; }
  h2 { font-size:15px; margin:0 0 10px; }
  .g { display:grid; grid-template-columns:repeat(auto-fit,minmax(140px,1fr)); gap:10px; }
  .g div { background:var(--p2); border-radius:8px; padding:10px; }
  .g span { display:block; color:var(--s); font-size:12px; }
  .g b { font-size:20px; }
  img { width:100%; border-radius:8px; background:#000; display:block; }
  button { font:inherit; border:0; border-radius:8px; padding:10px 14px; cursor:pointer; color:#fff;
           background:#2d5b8a; font-weight:600; margin:3px 3px 3px 0; }
  button.perigo { background:#9c2b2b; } button.sec { background:var(--p2); color:var(--t); }
  input { font:inherit; padding:10px; border-radius:8px; border:1px solid var(--b); background:var(--p2); color:var(--t); }
  .chip { font-size:12px; padding:4px 10px; border-radius:20px; background:var(--p2); }
  .ok { color:#9be3b8; background:#1b3a29; } .av { color:#ffe08a; background:#3b3218; } .er { color:#ffb1b1; background:#46201f; }
  .barra { height:20px; background:var(--p2); border-radius:10px; overflow:hidden; margin:8px 0; }
  .barra i { display:block; height:100%; background:linear-gradient(90deg,#2d5b8a,#4aa3ff); }
  .s { color:var(--s); font-size:13px; }
  table { width:100%; border-collapse:collapse; font-size:13px; }
  td { padding:4px 6px; border-bottom:1px solid var(--b); }
  a { color:var(--a); }
</style>
</head>
<body>
<main>
<?php if (!logado()): ?>
  <div class="c" style="max-width:360px;margin:60px auto">
    <h1>Impressora</h1>
    <form method="post">
      <input type="password" name="senha" placeholder="Senha" autofocus style="width:100%">
      <p><button>Entrar</button></p>
      <?php if ($erro): ?><p class="s" style="color:#ff7b72"><?= $h($erro) ?></p><?php endif ?>
    </form>
  </div>
<?php else: ?>
  <h1>Impressora <span><span class="chip" id="chip">…</span> <a class="s" href="?sair">sair</a></span></h1>
  <?php if ($msg): ?><div class="c"><?= $h($msg) ?></div><?php endif ?>

  <div class="c" id="impC" hidden>
    <h2 id="impNome"></h2>
    <div class="barra"><i id="impBarra" style="width:0"></i></div>
    <div class="s" id="impTxt"></div>
  </div>

  <div class="c">
    <h2>Agora</h2>
    <div class="g">
      <div><span>Bico</span><b id="bico">–</b></div>
      <div><span>Mesa</span><b id="mesa">–</b></div>
      <div><span>Impressora</span><b id="rele">–</b></div>
      <div><span>Última notícia</span><b id="visto">–</b></div>
    </div>
  </div>

  <div class="c">
    <h2>Câmera <span class="s" id="fotoHora"></span></h2>
    <img id="foto" alt="sem foto ainda">
  </div>

  <div class="c">
    <h2>Comandos</h2>
    <form method="post" onsubmit="return confirm(this.acao.options[this.acao.selectedIndex].text + '?')">
      <input type="hidden" name="token" value="<?= $h(token()) ?>">
      <select name="acao" style="font:inherit;padding:10px;border-radius:8px;background:#272d34;color:#e6e9ec;border:1px solid #3a424c">
        <?php foreach (ACOES as $k => $t): ?><option value="<?= $h($k) ?>"><?= $h($t) ?></option><?php endforeach ?>
      </select>
      <label class="s"><input type="checkbox" name="forcar" value="1"> forçar (não esperar esfriar / mesmo imprimindo)</label>
      <p><button class="perigo">Enviar comando</button></p>
    </form>
    <table id="cmds"></table>
  </div>
  <p class="s">Os dados chegam da ESP32-S3-CAM a cada ~20 s. Esta página atualiza sozinha.</p>
<script>
const $ = (i) => document.getElementById(i);
const dur = (s) => s == null || s < 0 ? '–' : s >= 3600 ? `${Math.floor(s / 3600)} h ${Math.floor(s % 3600 / 60)} min` : `${Math.round(s / 60)} min`;
const hora = (t) => new Date(t * 1000).toLocaleTimeString('pt-BR');
async function atualiza() {
  try {
    const d = await (await fetch('?json=1', { cache: 'no-store' })).json();
    const s = d.status, idade = s ? d.agora - s.recebido : 1e9;
    const on = idade < <?= (int) $CFG['SILENCIO_S'] ?>;
    $('chip').className = 'chip ' + (on ? 'ok' : 'er');
    $('chip').textContent = on ? 'placa no ar' : 'placa sem contato';
    if (s) {
      const tb = s.tb || {};
      $('bico').textContent = tb.bico != null ? `${tb.bico.toFixed(0)}/${tb.bicoAlvo} °C` : '–';
      $('mesa').textContent = tb.mesa != null ? `${tb.mesa.toFixed(0)}/${tb.mesaAlvo} °C` : '–';
      $('rele').textContent = s.rl ? (s.rl.impressoraDesligada ? 'desligada' : s.rl.aguardando ? 'desligando…' : 'ligada') : '–';
      $('visto').textContent = idade < 120 ? `há ${idade} s` : hora(s.recebido);
      const p = s.imp || {}, ativa = ['imprimindo', 'pausando', 'pausada'].includes(p.estado);
      $('impC').hidden = !(ativa || p.tft);
      if (ativa) {
        const pct = p.total ? p.bytes / p.total * 100 : 0;
        $('impNome').textContent = p.nome;
        $('impBarra').style.width = pct + '%';
        $('impTxt').textContent = `${pct.toFixed(1)}% · camada ${p.camada}/${p.camadas} · decorrido ${dur(p.decorrido)} · restam ${dur(p.restante)} · ${p.estado}`;
      } else if (p.tft) {
        const pct = p.tftCamadas ? p.tftCamada / p.tftCamadas * 100 : 0;
        $('impNome').textContent = p.tftNome + ' (pela TFT)';
        $('impBarra').style.width = pct + '%';
        $('impTxt').textContent = `camada ${p.tftCamada}/${p.tftCamadas} · decorrido ${dur(p.tftDecorrido)}`;
      }
    }
    if (d.foto && $('foto').dataset.t != d.foto) {
      $('foto').src = '?foto=1&t=' + d.foto;
      $('foto').dataset.t = d.foto;
      $('fotoHora').textContent = '· ' + hora(d.foto);
    }
    $('cmds').innerHTML = d.comandos.map((c) => `<tr><td>${hora(c.pedido)}</td><td>${c.acao}${c.forcar ? ' (forçado)' : ''}</td>` +
      `<td>${c.feito ? '✔ ' + (c.resultado || '') : c.entregue ? 'entregue' : 'aguardando a placa'}</td></tr>`).join('');
  } catch (e) { $('chip').textContent = 'erro'; }
}
atualiza();
setInterval(atualiza, 10000);
</script>
<?php endif ?>
</main>
</body>
</html>
