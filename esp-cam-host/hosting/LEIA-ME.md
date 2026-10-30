# Site da impressora (hospedagem PHP)

Página com o status da impressora, a última foto e os comandos dos relés, para
abrir de qualquer lugar. A ESP32-S3-CAM é quem procura o site (a cada ~20 s):
manda o estado, uma foto por minuto durante a impressão e busca os comandos
pendentes. O site nunca acessa a placa, então não é preciso abrir porta no roteador.

Não usa banco de dados: tudo fica em arquivos na pasta `dados/`.

## Instalar

1. Copie `config.exemplo.php` para **`config.php`** e preencha:
   - `CHAVE`: um segredo longo. Gere com `php -r "echo bin2hex(random_bytes(24));"`.
   - `SENHA_HASH`: a senha do site. Gere com `php -r "echo password_hash('sua senha', PASSWORD_DEFAULT);"`.
   Sem PHP no computador: crie um arquivo temporário `gera.php` com
   `<?php echo password_hash('sua senha', PASSWORD_DEFAULT); echo '<br>'; echo bin2hex(random_bytes(24));`,
   abra no navegador, copie e **apague o arquivo**.
2. Envie `index.php`, `api.php`, `comum.php` e `config.php` para uma pasta da
   hospedagem (ex.: `public_html/impressora/`) pelo Gerenciador de Arquivos ou FTP.
3. Abra `https://seudominio/impressora/`: deve pedir a senha.
4. Na página da placa: **Sistema → Nuvem**, informe o endereço da API
   (`https://seudominio/impressora/api.php`) e a mesma `CHAVE`.

Requisitos: PHP 7.4 ou mais novo, HTTPS. A pasta `dados/` é criada sozinha, com
uma `.htaccess` que bloqueia o acesso direto (Apache).

## Segurança

- A senha do site fica só como hash; a `CHAVE` só no `config.php` e na placa.
  `config.php` **não** vai para o Git (`.gitignore`).
- Os comandos pedem confirmação e usam token contra envio por outro site (CSRF).
- O comando chega à placa como pedido: ela aplica as mesmas regras da página
  local (espera o bico esfriar, recusa durante a impressão sem "forçar").
