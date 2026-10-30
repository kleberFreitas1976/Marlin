<?php
// Funcoes comuns ao site e a API.
declare(strict_types=1);

$CFG = require __DIR__ . '/config.php';
if (!is_dir($CFG['DADOS'])) {
    mkdir($CFG['DADOS'], 0770, true);
}
// bloqueia acesso direto aos dados (Apache)
$ht = $CFG['DADOS'] . '/.htaccess';
if (!file_exists($ht)) {
    file_put_contents($ht, "Require all denied\nDeny from all\n");
}

function caminho(string $nome): string
{
    global $CFG;
    return $CFG['DADOS'] . '/' . $nome;
}

function le_json(string $nome, $padrao)
{
    $c = caminho($nome);
    if (!file_exists($c)) {
        return $padrao;
    }
    $d = json_decode((string) file_get_contents($c), true);
    return $d ?? $padrao;
}

// Grava com trava: a API e o site podem escrever ao mesmo tempo
function grava_json(string $nome, $dados): void
{
    $c = caminho($nome);
    $tmp = $c . '.' . bin2hex(random_bytes(4));
    file_put_contents($tmp, json_encode($dados, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT));
    rename($tmp, $c);
}

function altera_json(string $nome, $padrao, callable $f)
{
    $fp = fopen(caminho($nome . '.lock'), 'c');
    flock($fp, LOCK_EX);
    $dados = le_json($nome, $padrao);
    $res = $f($dados);
    grava_json($nome, $dados);
    flock($fp, LOCK_UN);
    fclose($fp);
    return $res;
}

function responde_json($dados, int $codigo = 200): void
{
    http_response_code($codigo);
    header('Content-Type: application/json; charset=utf-8');
    header('Cache-Control: no-store');
    echo json_encode($dados, JSON_UNESCAPED_UNICODE);
    exit;
}

function sessao(): void
{
    session_set_cookie_params(['httponly' => true, 'secure' => !empty($_SERVER['HTTPS']), 'samesite' => 'Strict']);
    session_start();
}

function logado(): bool
{
    return !empty($_SESSION['ok']);
}

// Token contra envio de formulario por outro site (CSRF)
function token(): string
{
    if (empty($_SESSION['token'])) {
        $_SESSION['token'] = bin2hex(random_bytes(16));
    }
    return $_SESSION['token'];
}

const ACOES = [
    'desliga_impressora' => 'Desligar a impressora (espera o bico esfriar)',
    'liga_impressora'    => 'Religar a impressora',
    'desliga_tudo'       => 'Desligar TUDO (espera o bico esfriar)',
    'cancela'            => 'Cancelar espera de desligamento',
    'foto'               => 'Tirar uma foto agora',
];
