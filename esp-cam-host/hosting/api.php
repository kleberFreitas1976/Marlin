<?php
// API usada pela ESP32-S3-CAM. Autenticacao: cabecalho X-Chave = CHAVE do config.php.
//
//   POST api.php?acao=status   corpo: JSON do estado. Resposta: comandos pendentes.
//   POST api.php?acao=foto     corpo: JPEG (ultima foto)
//   POST api.php?acao=feito&id=ID   a placa executou o comando ID
declare(strict_types=1);
require __DIR__ . '/comum.php';

$chave = $_SERVER['HTTP_X_CHAVE'] ?? '';
if (!hash_equals($CFG['CHAVE'], $chave)) {
    responde_json(['erro' => 'chave invalida'], 403);
}
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    responde_json(['erro' => 'use POST'], 405);
}
$corpo = (string) file_get_contents('php://input');
$acao = $_GET['acao'] ?? '';

switch ($acao) {
    case 'status':
        $e = json_decode($corpo, true);
        if (!is_array($e)) {
            responde_json(['erro' => 'JSON invalido'], 400);
        }
        $e['recebido'] = time();
        grava_json('status.json', $e);
        // entrega os comandos ainda nao entregues
        $pend = altera_json('comandos.json', [], function (&$cmds) {
            $out = [];
            foreach ($cmds as &$c) {
                if (empty($c['entregue'])) {
                    $c['entregue'] = time();
                    $out[] = ['id' => $c['id'], 'acao' => $c['acao'], 'forcar' => !empty($c['forcar'])];
                }
            }
            // guarda so os ultimos 50
            $cmds = array_slice($cmds, -50);
            return $out;
        });
        responde_json(['ok' => true, 'comandos' => $pend]);

    case 'foto':
        // JPEG inteiro: comeca com FF D8 e termina com FF D9. Um envio interrompido
        // (Wi-Fi fraco) chega cortado e nao pode substituir a ultima foto boa.
        if (strlen($corpo) < 100 || substr($corpo, 0, 2) !== "\xFF\xD8" || substr($corpo, -2) !== "\xFF\xD9") {
            responde_json(['erro' => 'JPEG incompleto'], 400);
        }
        file_put_contents(caminho('ultima.jpg.tmp'), $corpo);
        rename(caminho('ultima.jpg.tmp'), caminho('ultima.jpg'));
        responde_json(['ok' => true]);

    case 'feito':
        $id = $_GET['id'] ?? '';
        $res = $_GET['resultado'] ?? 'ok';
        altera_json('comandos.json', [], function (&$cmds) use ($id, $res) {
            foreach ($cmds as &$c) {
                if ($c['id'] === $id) {
                    $c['feito'] = time();
                    $c['resultado'] = substr($res, 0, 120);
                }
            }
        });
        responde_json(['ok' => true]);

    default:
        responde_json(['erro' => 'acao desconhecida'], 400);
}
