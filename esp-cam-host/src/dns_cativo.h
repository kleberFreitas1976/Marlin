#pragma once

// DNS do portal cativo: enquanto o AP de configuracao esta ligado, responde
// qualquer nome com o IP da placa. Assim o celular abre a pagina sozinho.
void dns_cativo_inicia(void);
void dns_cativo_para(void);
