<?php
// Copie para config.php (NAO versionar) e preencha.
//
// CHAVE: segredo compartilhado com a ESP32-S3-CAM (o mesmo valor vai na
//   pagina da placa, aba Sistema > Nuvem). Gere um longo e aleatorio, por ex.:
//   php -r "echo bin2hex(random_bytes(24));"
// SENHA_HASH: hash da senha do site. Gere com:
//   php -r "echo password_hash('sua senha', PASSWORD_DEFAULT);"
return [
    'CHAVE'      => 'troque-por-um-segredo-longo',
    'SENHA_HASH' => '$2y$10$troque.pelo.hash.gerado.com.password_hash............',
    // pasta dos dados (status, fotos, comandos). Fora da pasta publica, se a
    // hospedagem permitir; senao, a .htaccess de dados/ bloqueia o acesso direto.
    'DADOS'      => __DIR__ . '/dados',
    // a placa e considerada fora do ar se nao der noticia por este tempo
    'SILENCIO_S' => 90,
];
