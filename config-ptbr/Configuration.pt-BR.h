/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Baseado no Sprinter e no grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * Este programa é software livre: você pode redistribuí-lo e/ou modificá-lo
 * sob os termos da Licença Pública Geral GNU, conforme publicada pela
 * Free Software Foundation, na versão 3 da licença ou (a seu critério)
 * qualquer versão posterior.
 *
 * Este programa é distribuído na esperança de ser útil, mas SEM NENHUMA
 * GARANTIA; sem sequer a garantia implícita de COMERCIALIZAÇÃO ou
 * ADEQUAÇÃO A UM FIM ESPECÍFICO. Veja a Licença Pública Geral GNU
 * para mais detalhes.
 *
 * Você deve ter recebido uma cópia da Licença Pública Geral GNU junto
 * com este programa. Se não, veja <https://www.gnu.org/licenses/>.
 *
 */
#pragma once

/**
 * ===========================================================================
 *  TRADUÇÃO PT-BR — ARQUIVO DE REFERÊNCIA, NÃO USADO NA COMPILAÇÃO
 * ===========================================================================
 * Cópia traduzida de Marlin/Configuration.h. O firmware é compilado a partir
 * do arquivo original; este aqui existe só para leitura e estudo.
 * ===========================================================================
 *
 * Configuration.h
 *
 * Ajustes básicos, como:
 *
 * - Tipo da eletrônica (placa)
 * - Tipo do sensor de temperatura
 * - Geometria da impressora
 * - Configuração dos fins de curso
 * - Controlador de LCD
 * - Recursos extras
 *
 * Ajustes avançados ficam em Configuration_adv.h
 */
#define CONFIGURATION_H_VERSION 02010300

//===========================================================================
//============================ Por onde começar =============================
//===========================================================================

/**
 * Links úteis para configurar e calibrar a sua máquina:
 *
 * Exemplos de config:  https://github.com/MarlinFirmware/Configurations/branches/all
 *
 * Calculadora Průša:   https://blog.prusa3d.com/calculator_3416/
 *
 * Guias de calibração: https://reprap.org/wiki/Calibration
 *                      https://reprap.org/wiki/Triffid_Hunter%27s_Calibration_Guide
 *                      https://web.archive.org/web/20220907014303/sites.google.com/site/repraplogphase/calibration-of-your-reprap
 *                      https://youtu.be/wAL9d7FgInk
 *                      https://teachingtechyt.github.io/calibration.html
 *
 * Objetos de calibração: https://www.thingiverse.com/thing:5573
 *                        https://www.thingiverse.com/thing:1278865
 */

// @section info

// Dados do autor desta compilação, enviados ao host no boot e pelo M115
#define STRING_CONFIG_H_AUTHOR "(MarlinFirmware)" // Autor ou colaborador original.
//#define CUSTOM_VERSION_FILE Version.h // Caminho a partir da raiz (sem aspas)

// @section machine

// Escolha, em boards.h, o nome que corresponde à sua montagem
#ifndef MOTHERBOARD
  #define MOTHERBOARD BOARD_MKS_TINYBEE
#endif

// @section serial

/**
 * Seleciona a porta serial da placa usada para falar com o host.
 * Isso permite ligar adaptadores sem fio (por exemplo) em pinos não padrão.
 * A porta serial -1 é a serial emulada por USB, quando existir.
 * Obs.: a primeira porta serial (-1 ou 0) é sempre usada pelo bootloader do Arduino.
 *
 * :[-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
 */
#define SERIAL_PORT 0

/**
 * Velocidade (baud rate) da porta serial
 * Esta é a velocidade padrão de comunicação de todas as portas seriais.
 * Defina abaixo os padrões das portas seriais adicionais.
 *
 * 250000 funciona na maioria dos casos, mas você pode tentar uma velocidade
 * menor se costuma ter quedas de comunicação imprimindo pelo host.
 * Pode-se chegar a 1000000 para acelerar a transferência de arquivos no SD.
 *
 * :[2400, 9600, 19200, 38400, 57600, 115200, 250000, 500000, 1000000]
 */
#define BAUDRATE 250000

//#define BAUD_RATE_GCODE     // Habilita o G-code M575 para ajustar a velocidade

/**
 * Seleciona uma segunda porta serial da placa para falar com o host.
 * Atualmente Ethernet (-2) só é suportado em placas Teensy 4.1.
 * :[-2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
 */
#define SERIAL_PORT_2 2      // MKS TFT24 V1.1 (modo touch / serial)
#define BAUDRATE_2 115200    // Baudrate padrão da MKS TFT24 V1.1

/**
 * Seleciona uma terceira porta serial da placa para falar com o host.
 * Atualmente suportado em AVR, DUE, SAMD51, LPC1768/9, STM32/STM32F1/HC32 e Teensy 4.x
 * :[-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
 */
//#define SERIAL_PORT_3 1
//#define BAUDRATE_3 250000   // :[2400, 9600, 19200, 38400, 57600, 115200, 250000, 500000, 1000000] Habilite para sobrepor BAUDRATE

/**
 * Seleciona uma porta serial para comunicação com protocolo RS485
 * :[-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
 */
//#define RS485_SERIAL_PORT 1
#ifdef RS485_SERIAL_PORT
  //#define M485_PROTOCOL 1   // Verifique a compatibilidade de protocolo com o seu host
  //#define RS485_BUS_BUFFER_SIZE 128
#endif

// Habilita a interface serial Bluetooth em dispositivos AT90USB
//#define BLUETOOTH

// Nome exibido na mensagem "Ready" do LCD e no menu de informações
#define CUSTOM_MACHINE_NAME "CoreXY TinyBee"
//#define CONFIGURABLE_MACHINE_NAME // Adiciona o G-code M550 para definir/consultar o nome da máquina

// ID único da impressora, usado por alguns programas para diferenciar máquinas.
// Escolha o seu ou use um serviço como https://www.uuidgenerator.net/version4
//#define MACHINE_UUID "00000000-0000-0000-0000-000000000000"

// @section stepper drivers

/**
 * Drivers dos motores de passo
 *
 * Estes ajustes permitem ao Marlin afinar a temporização dos drivers e habilitar
 * opções avançadas nos drivers que as suportam. Você também pode sobrepor as
 * opções de temporização em Configuration_adv.h.
 *
 * Use TMC2208/TMC2208_STANDALONE para drivers TMC2225 e TMC2209/TMC2209_STANDALONE para TMC2226.
 *
 * Opções: A4988, A5984, DRV8825, LV8729, TB6560, TB6600, TMC2100,
 *         TMC2130, TMC2130_STANDALONE, TMC2160, TMC2160_STANDALONE,
 *         TMC2208, TMC2208_STANDALONE, TMC2209, TMC2209_STANDALONE,
 *         TMC2240, TMC2660, TMC2660_STANDALONE,
 *         TMC5130, TMC5130_STANDALONE, TMC5160, TMC5160_STANDALONE
 * :['A4988', 'A5984', 'DRV8825', 'LV8729', 'TB6560', 'TB6600', 'TMC2100', 'TMC2130', 'TMC2130_STANDALONE', 'TMC2160', 'TMC2160_STANDALONE', 'TMC2208', 'TMC2208_STANDALONE', 'TMC2209', 'TMC2209_STANDALONE', 'TMC2240', 'TMC2660', 'TMC2660_STANDALONE', 'TMC5130', 'TMC5130_STANDALONE', 'TMC5160', 'TMC5160_STANDALONE']
 */
#define X_DRIVER_TYPE  TMC2209_STANDALONE
#define Y_DRIVER_TYPE  TMC2209_STANDALONE
#define Z_DRIVER_TYPE  TMC2209_STANDALONE   // Motor Z da DIREITA
//#define X2_DRIVER_TYPE A4988
//#define Y2_DRIVER_TYPE A4988
#define Z2_DRIVER_TYPE TMC2209_STANDALONE   // Motor Z da ESQUERDA, ligado no driver E1
//#define Z3_DRIVER_TYPE A4988
//#define Z4_DRIVER_TYPE A4988
//#define I_DRIVER_TYPE  A4988
//#define J_DRIVER_TYPE  A4988
//#define K_DRIVER_TYPE  A4988
//#define U_DRIVER_TYPE  A4988
//#define V_DRIVER_TYPE  A4988
//#define W_DRIVER_TYPE  A4988
#define E0_DRIVER_TYPE TMC2209_STANDALONE
//#define E1_DRIVER_TYPE A4988
//#define E2_DRIVER_TYPE A4988
//#define E3_DRIVER_TYPE A4988
//#define E4_DRIVER_TYPE A4988
//#define E5_DRIVER_TYPE A4988
//#define E6_DRIVER_TYPE A4988
//#define E7_DRIVER_TYPE A4988

/**
 * Ajustes de eixos adicionais
 *
 * Defina AXISn_ROTATES para todos os eixos que giram ou pivotam.
 * As coordenadas de eixos rotativos são expressas em graus.
 *
 * AXISn_NAME define a letra usada para referenciar o eixo na maioria dos G-codes.
 * Por convenção, os nomes e as funções costumam ser:
 *   'A' : eixo rotativo paralelo ao X
 *   'B' : eixo rotativo paralelo ao Y
 *   'C' : eixo rotativo paralelo ao Z
 *   'U' : eixo linear secundário paralelo ao X
 *   'V' : eixo linear secundário paralelo ao Y
 *   'W' : eixo linear secundário paralelo ao Z
 *
 * Independente destes ajustes, internamente os eixos se chamam I, J, K, U, V, W.
 */
#ifdef I_DRIVER_TYPE
  #define AXIS4_NAME 'A' // :['A', 'B', 'C', 'U', 'V', 'W']
  #define AXIS4_ROTATES
#endif
#ifdef J_DRIVER_TYPE
  #define AXIS5_NAME 'B' // :['B', 'C', 'U', 'V', 'W']
  #define AXIS5_ROTATES
#endif
#ifdef K_DRIVER_TYPE
  #define AXIS6_NAME 'C' // :['C', 'U', 'V', 'W']
  #define AXIS6_ROTATES
#endif
#ifdef U_DRIVER_TYPE
  #define AXIS7_NAME 'U' // :['U', 'V', 'W']
  //#define AXIS7_ROTATES
#endif
#ifdef V_DRIVER_TYPE
  #define AXIS8_NAME 'V' // :['V', 'W']
  //#define AXIS8_ROTATES
#endif
#ifdef W_DRIVER_TYPE
  #define AXIS9_NAME 'W' // :['W']
  //#define AXIS9_ROTATES
#endif

// @section extruder

// Define o número de extrusoras
// :[0, 1, 2, 3, 4, 5, 6, 7, 8]
#define EXTRUDERS 1

// Diâmetro de filamento normalmente esperado (1.75, 2.85, 3.0, ...). Usado por
// extrusão volumétrica, sensor de largura de filamento, etc.
#define DEFAULT_NOMINAL_FILAMENT_DIA 1.75

// Para Cyclops ou qualquer "multi-extrusora" que compartilhe um único bico.
//#define SINGLENOZZLE

// Salva e restaura temperatura e velocidade da ventoinha na troca de ferramenta.
// Defina o modo de espera da ferramenta não selecionada com M104/106/109 T...
#if ENABLED(SINGLENOZZLE)
  //#define SINGLENOZZLE_STANDBY_TEMP
  //#define SINGLENOZZLE_STANDBY_FAN
#endif

// Uma extrusora dupla que usa um único motor de passo
//#define SWITCHING_EXTRUDER
#if ENABLED(SWITCHING_EXTRUDER)
  #define SWITCHING_EXTRUDER_SERVO_NR 0
  #define SWITCHING_EXTRUDER_SERVO_ANGLES { 0, 90 } // Ângulos para E0, E1[, E2, E3]
  #if EXTRUDERS > 3
    #define SWITCHING_EXTRUDER_E23_SERVO_NR 1
  #endif
#endif

// Troca de extrusora esbarrando o cabeçote. Requer EVENT_GCODE_TOOLCHANGE_#.
//#define MECHANICAL_SWITCHING_EXTRUDER

/**
 * Bico duplo que usa um servomotor para subir/descer um (ou ambos) os bicos.
 * Pode ser combinado com SWITCHING_EXTRUDER.
 */
//#define SWITCHING_NOZZLE
#if ENABLED(SWITCHING_NOZZLE)
  #define SWITCHING_NOZZLE_SERVO_NR 0
  //#define SWITCHING_NOZZLE_E1_SERVO_NR 1          // Se forem usados dois servos, o índice do segundo
  #define SWITCHING_NOZZLE_SERVO_ANGLES { 0, 90 }   // Um par de ângulos para { E0, E1 }.
                                                    // Com dois servos use dois pares: { { desce, sobe }, { desce, sobe } }
  #define SWITCHING_NOZZLE_SERVO_DWELL 2500         // Tempo de espera para o servo completar o movimento físico
  #define SWITCHING_NOZZLE_LIFT_TO_PROBE            // Levanta os cabeçotes para fora do caminho durante a sondagem
#endif

// Troca de bico esbarrando o cabeçote. Requer EVENT_GCODE_TOOLCHANGE_#.
//#define MECHANICAL_SWITCHING_NOZZLE

/**
 * Dois carros X independentes, com extrusoras que se acoplam a uma parte móvel
 * por um mecanismo de encaixe com solenoide. Requer SOL1_PIN e SOL2_PIN.
 */
//#define PARKING_EXTRUDER

/**
 * Dois carros X independentes, com extrusoras que se acoplam a uma parte móvel
 * por um mecanismo de encaixe magnético, usando movimentos e sem solenoide.
 *
 * projeto    : https://www.thingiverse.com/thing:3080893
 * movimentos : https://youtu.be/0xCEiG9VS3k
 *              https://youtu.be/Bqbcs0CU2FE
 */
//#define MAGNETIC_PARKING_EXTRUDER

#if ANY(PARKING_EXTRUDER, MAGNETIC_PARKING_EXTRUDER)

  #define PARKING_EXTRUDER_PARKING_X { -78, 184 }     // Posições X para estacionar as extrusoras
  #define PARKING_EXTRUDER_GRAB_DISTANCE 1            // (mm) Distância a avançar além do ponto de estacionamento para agarrar a extrusora

  #if ENABLED(PARKING_EXTRUDER)

    #define PARKING_EXTRUDER_SOLENOIDS_INVERT           // Se habilitado, o solenoide NÃO é magnetizado com tensão aplicada
    #define PARKING_EXTRUDER_SOLENOIDS_PINS_ACTIVE LOW  // Sinal LOW ou HIGH no pino energiza a bobina
    #define PARKING_EXTRUDER_SOLENOIDS_DELAY 250        // (ms) Atraso para o campo magnético. Sem atraso se 0 ou não definido.
    //#define MANUAL_SOLENOID_CONTROL                   // Controle manual dos solenoides de encaixe com M380 S / M381

  #elif ENABLED(MAGNETIC_PARKING_EXTRUDER)

    #define MPE_FAST_SPEED      9000      // (mm/min) Velocidade do percurso antes do último trecho
    #define MPE_SLOW_SPEED      4500      // (mm/min) Velocidade do último trecho até estacionar e acoplar
    #define MPE_TRAVEL_DISTANCE   10      // (mm) Comprimento do último trecho
    #define MPE_COMPENSATION       0      // Compensação de offset -1 , 0 , 1 (multiplicador), só para o acoplamento

  #endif

#endif

/**
 * Extrusora diferencial
 *
 * Os motores X e E trabalham juntos formando um sistema de acionamento diferencial.
 * Simples     : passos E = X + E   ; passos X = X  (E move uma malha, X não muda)
 * Balanceado  : passos E = X + E/2 ; passos X = X - E/2  (sistema de malha dupla)
 */
//#define DIFFERENTIAL_EXTRUDER
#if ENABLED(DIFFERENTIAL_EXTRUDER)
  //#define BALANCED_DIFFERENTIAL_EXTRUDER
#endif

/**
 * Cabeçote intercambiável (Switching Toolhead)
 *
 * Suporte a cabeçotes trocáveis e encaixáveis, como o
 * E3D Tool Changer. Os cabeçotes são travados por um servo.
 */
//#define SWITCHING_TOOLHEAD

/**
 * Cabeçote intercambiável magnético
 *
 * Suporte a cabeçotes trocáveis e encaixáveis com mecanismo de
 * encaixe magnético, usando movimento e sem servo.
 */
//#define MAGNETIC_SWITCHING_TOOLHEAD

/**
 * Cabeçote intercambiável eletromagnético
 *
 * Estacionamento para cinemática CoreXY / HBot.
 * Os cabeçotes ficam estacionados numa borda e presos por um eletroímã.
 * Suporta mais de 2 cabeçotes. Veja https://youtu.be/JolbsAKTKf4
 */
//#define ELECTROMAGNETIC_SWITCHING_TOOLHEAD

#if ANY(SWITCHING_TOOLHEAD, MAGNETIC_SWITCHING_TOOLHEAD, ELECTROMAGNETIC_SWITCHING_TOOLHEAD)
  #define SWITCHING_TOOLHEAD_Y_POS          235         // (mm) Posição Y da doca do cabeçote
  #define SWITCHING_TOOLHEAD_Y_SECURITY      10         // (mm) Distância de segurança no eixo Y
  #define SWITCHING_TOOLHEAD_Y_CLEAR         60         // (mm) Distância mínima da doca para o eixo X ficar livre
  #define SWITCHING_TOOLHEAD_X_POS          { 215, 0 }  // (mm) Posições X para estacionar as extrusoras
  #if ENABLED(SWITCHING_TOOLHEAD)
    #define SWITCHING_TOOLHEAD_SERVO_NR       2         // Índice do conector do servo
    #define SWITCHING_TOOLHEAD_SERVO_ANGLES { 0, 180 }  // (graus) Ângulos para travar e destravar
  #elif ENABLED(MAGNETIC_SWITCHING_TOOLHEAD)
    #define SWITCHING_TOOLHEAD_Y_RELEASE      5         // (mm) Distância de segurança no eixo Y
    #define SWITCHING_TOOLHEAD_X_SECURITY   { 90, 150 } // (mm) Distância de segurança no eixo X (T0,T1)
    //#define PRIME_BEFORE_REMOVE                       // Purga o bico antes de soltar da doca
    #if ENABLED(PRIME_BEFORE_REMOVE)
      #define SWITCHING_TOOLHEAD_PRIME_MM           20  // (mm)   Comprimento de purga da extrusora
      #define SWITCHING_TOOLHEAD_RETRACT_MM         10  // (mm)   Comprimento de retração após a purga
      #define SWITCHING_TOOLHEAD_PRIME_FEEDRATE    300  // (mm/min) Velocidade de purga da extrusora
      #define SWITCHING_TOOLHEAD_RETRACT_FEEDRATE 2400  // (mm/min) Velocidade de retração da extrusora
    #endif
  #elif ENABLED(ELECTROMAGNETIC_SWITCHING_TOOLHEAD)
    #define SWITCHING_TOOLHEAD_Z_HOP          2         // (mm) Subida de Z para a troca
  #endif
#endif

/**
 * "Extrusora misturadora" (Mixing Extruder)
 *   - Adiciona os G-codes M163 e M164 para definir e "confirmar" os fatores de mistura atuais.
 *   - Estende as rotinas de passo para mover vários motores em proporção à mistura.
 *   - Suporte opcional ao 'M164 S<índice>' do firmware Repetier, com ferramentas virtuais.
 *   - Esta implementação suporta até duas extrusoras misturadoras.
 *   - Habilite DIRECT_MIXING_IN_G1 para M165 e mistura dentro do G1 (da implementação de referência da Pia Taubert).
 */
//#define MIXING_EXTRUDER
#if ENABLED(MIXING_EXTRUDER)
  #define MIXING_STEPPERS 2        // Número de motores na sua extrusora misturadora
  #define MIXING_VIRTUAL_TOOLS 16  // Usa o método de ferramenta virtual com M163 e M164
  //#define DIRECT_MIXING_IN_G1    // Permite fatores de mistura ABCDHI nos comandos de movimento G1
  //#define GRADIENT_MIX           // Suporte a mistura em gradiente com M166 e LCD
  //#define MIXING_PRESETS         // Define 8 presets padrão de V-tool para 2 ou 3 MIXING_STEPPERS
  #if ENABLED(GRADIENT_MIX)
    //#define GRADIENT_VTOOL       // Adiciona M166 T para usar um índice de V-tool como apelido do gradiente
  #endif
#endif

// Offset das extrusoras (descomente se usar mais de uma e depender do firmware para posicionar na troca).
// O offset tem que ser X=0, Y=0 para o hotend da extrusora 0 (extrusora padrão).
// Para os demais hotends é a distância deles até o hotend da extrusora 0.
//#define HOTEND_OFFSET_X { 0.0, 20.00 } // (mm) offset X relativo de cada bico
//#define HOTEND_OFFSET_Y { 0.0, 5.00 }  // (mm) offset Y relativo de cada bico
//#define HOTEND_OFFSET_Z { 0.0, 0.00 }  // (mm) offset Z relativo de cada bico

// @section multi-material

/**
 * Unidade multimaterial (MMU)
 * Defina um destes modelos pré-definidos:
 *
 *   PRUSA_MMU1           : Průša MMU1 (a versão "multiplexadora")
 *   PRUSA_MMU2           : Průša MMU2
 *   PRUSA_MMU2S          : Průša MMU2S (requer extrusora MK3S com sensor de movimento, EXTRUDERS = 5)
 *   PRUSA_MMU3           : Průša MMU3  (requer extrusora MK3S com sensor de movimento e firmware MMU versão 3.x.x, EXTRUDERS = 5)
 *   EXTENDABLE_EMU_MMU2  : MMU com número configurável de filamentos (ERCF, SMuFF ou similar com firmware compatível com o Průša MMU2)
 *   EXTENDABLE_EMU_MMU2S : MMUS com número configurável de filamentos (ERCF, SMuFF ou similar com firmware compatível com o Průša MMU2)
 *
 * Requer NOZZLE_PARK_FEATURE para estacionar o cabeçote caso a unidade MMU falhe.
 * Veja opções adicionais em Configuration_adv.h.
 * :["PRUSA_MMU1", "PRUSA_MMU2", "PRUSA_MMU2S", "PRUSA_MMU3", "EXTENDABLE_EMU_MMU2", "EXTENDABLE_EMU_MMU2S"]
 */
//#define MMU_MODEL PRUSA_MMU3

// @section psu control

/**
 * Controle da fonte de alimentação
 *
 * Habilite e ligue a fonte ao pino PS_ON_PIN.
 * Informe se a fonte é acionada em nível alto (HIGH) ou baixo (LOW).
 */
//#define PSU_CONTROL
//#define PSU_NAME "Power Supply"

#if ENABLED(PSU_CONTROL)
  //#define MKS_PWC                 // Usando o módulo adicional MKS PWC
  //#define PS_OFF_CONFIRM          // Caixa de confirmação ao desligar
  //#define PS_OFF_SOUND            // Bipe de 1s ao desligar
  #define PSU_ACTIVE_STATE LOW      // Use 'LOW' para ATX, 'HIGH' para X-Box

  //#define PSU_DEFAULT_OFF             // Mantém a fonte desligada até ser ligada diretamente com M80
  //#define PSU_POWERUP_DELAY      250  // (ms) Espera para a fonte atingir a potência plena
  //#define LED_POWEROFF_TIMEOUT 10000  // (ms) Apaga os LEDs depois de desligar, com este atraso

  //#define PSU_OFF_REDUNDANT           // Segundo pino para controle redundante de energia
  //#define PSU_OFF_REDUNDANT_INVERTED  // O estado do pino redundante é o inverso de PSU_ACTIVE_STATE

  //#define PS_ON1_PIN               6  // Pino redundante necessário para ligar junto com o PS_ON_PIN

  //#define PS_ON_EDM_PIN            8  // Pinos de monitoramento externo para realimentação do relé de energia. Falha se divergir.
  //#define PS_ON1_EDM_PIN           9
  #define PS_EDM_RESPONSE          250  // (ms) Tempo concedido para a ação do relé

  //#define POWER_OFF_TIMER               // Habilita M81 D<segundos> para desligar após um atraso
  //#define POWER_OFF_WAIT_FOR_COOLDOWN   // Habilita M81 S para desligar só depois de esfriar

  //#define PSU_POWERUP_GCODE  "M355 S1"  // G-code a executar após ligar (ex.: acender a luz do gabinete)
  //#define PSU_POWEROFF_GCODE "M355 S0"  // G-code a executar antes de desligar (ex.: apagar a luz do gabinete)

  //#define AUTO_POWER_CONTROL      // Habilita o controle automático do pino PS_ON
  #if ENABLED(AUTO_POWER_CONTROL)
    #define AUTO_POWER_FANS           // Liga a fonte para as ventoinhas
    #define AUTO_POWER_E_FANS         // Liga a fonte para as ventoinhas da extrusora
    #define AUTO_POWER_CONTROLLERFAN  // Liga a fonte para a ventoinha da eletrônica
    #define AUTO_POWER_CHAMBER_FAN    // Liga a fonte para a ventoinha da câmara
    #define AUTO_POWER_COOLER_FAN     // Liga a fonte para a ventoinha do cooler
    #define AUTO_POWER_SPINDLE_LASER  // Liga a fonte para o spindle/laser
    #define POWER_TIMEOUT              30 // (s) Desliga a energia se a máquina ficar ociosa por este tempo
    //#define POWER_OFF_DELAY          60 // (s) Atraso do desligamento após o comando M81. Útil para deixar as ventoinhas girando mais um pouco.
  #endif
  #if ANY(AUTO_POWER_CONTROL, POWER_OFF_WAIT_FOR_COOLDOWN)
    //#define AUTO_POWER_E_TEMP        50 // (°C) Fonte ligada se alguma extrusora estiver acima desta temperatura
    //#define AUTO_POWER_CHAMBER_TEMP  30 // (°C) Fonte ligada se a câmara estiver acima desta temperatura
    //#define AUTO_POWER_COOLER_TEMP   26 // (°C) Fonte ligada se o cooler estiver acima desta temperatura
  #endif
#endif

//===========================================================================
//========================= Ajustes de temperatura ==========================
//===========================================================================
// @section temperature sensors

/**
 * Sensores de temperatura:
 *
 * O NORMAL É PULL-UP DE 4,7 kΩ! Sensores de hotend podem usar pull-up de 1 kΩ com o resistor e a tabela corretos.
 *
 * ================================================================
 *  Termistores analógicos - pull-up 4,7 kΩ - Normal
 * ================================================================
 *     1 : 100kΩ EPCOS - melhor escolha para termistores EPCOS
 *   331 : 100kΩ Igual ao #1, mas escalado para 3,3V no MEGA
 *   332 : 100kΩ Igual ao #1, mas escalado para 3,3V no DUE
 *     2 : 200kΩ ATC Semitec 204GT-2
 *   202 : 200kΩ Copymaster 3D
 *     3 : ???Ω  Termistor Mendel-parts
 *     4 : 10kΩ  Termistor genérico !! NÃO use em hotend - dá resolução ruim em alta temperatura. !!
 *     5 : 100kΩ ATC Semitec 104GT-2/104NT-4-R025H42G - usado em ParCan, J-Head e E3D, SliceEngineering 300°C
 *   501 : 100kΩ Zonestar - Tronxy X3A
 *   502 : 100kΩ Zonestar - usado na mesa aquecida da Zonestar Průša P802M
 *   503 : 100kΩ Zonestar (Z8XM2) termistor da mesa aquecida
 *   504 : 100kΩ Zonestar P802QR2 (cód. QWG-104F-B3950) termistor do hotend
 *   505 : 100kΩ Zonestar P802QR2 (cód. QWG-104F-3950) termistor da mesa
 *   512 : 100kΩ hotend RPW-Ultra
 *     6 : 100kΩ EPCOS - menos preciso que a tabela #1 (criada com um termopar Fluke)
 *     7 : 100kΩ Honeywell 135-104LAG-J01
 *    71 : 100kΩ Honeywell 135-104LAF-J01
 *     8 : 100kΩ Vishay 0603 SMD NTCS0603E3104FXT
 *     9 : 100kΩ GE Sensing AL03006-58.2K-97-G1
 *    10 : 100kΩ RS PRO 198-961
 *    11 : 100kΩ Mantas de silicone Keenovo AC, maioria das Wanhao i3 - beta 3950, 1%
 *    12 : 100kΩ Vishay 0603 SMD NTCS0603E3104FXT (#8) - calibrado para a mesa da Makibox
 *    13 : 100kΩ Hisense até 300°C - para hotends "Simple ONE" e "All In ONE" - R25 = 100 kOhm, beta25 = 4100 K, pull-up 4,7 kΩ
 *    14 : 100kΩ (R25), 4092K (beta25), pull-up 4,7 kΩ, termistor de mesa (usado na Ender-5 S1)
 *    15 : 100kΩ calibrado para o hotend da JGAurora A5
 *    17 : 100kΩ termistor branco NTC da Dagoma
 *    18 : 200kΩ ATC Semitec 204GT-2 Dagoma.Fr - MKS_Base_DKU001327
 *    22 : 100kΩ GTM32 Pro vB - hotend - pull-up 4,7 kΩ para 3,3V e 220Ω para a entrada analógica
 *    23 : 100kΩ GTM32 Pro vB - mesa - pull-up 4,7 kΩ para 3,3V e 220Ω para a entrada analógica
 *    30 : 100kΩ Manta de aquecimento de silicone Kis3d 200W/300W com placa fundida de precisão de 6mm (EN AW 5083) NTC100K - beta 3950
 *    60 : 100kΩ Termistor de mesa Kapton da Maker's Tool Works - beta 3950
 *    61 : 100kΩ Termistor Formbot/Vivedino 350°C - beta 3950
 *    66 : 4.7MΩ Termistor de alta temperatura Dyze Design / Trianglelab T-D500 500°C
 *    67 : 500kΩ Termistor SliceEngineering 450°C
 *    68 : Placa amplificadora PT100 da Dyze Design
 *    70 : 100kΩ bq Hephestos 2
 *    75 : 100kΩ Manta de silicone genérica com NTC100K MGB18-104F39050L32
 *   666 : 200kΩ Termistor personalizado da Einstart S com pull-up de 10k.
 *  2000 : 100kΩ Termistor NTC100K da placa-mãe Ultimachine Rambo TDK NTCG104LH104KT1
 *
 * ================================================================
 *  Termistores analógicos - pull-up 1 kΩ
 *   Atípico, e exige trocar o pull-up de 4,7 kΩ por 1 kΩ.
 *   (mas dá mais precisão e PID mais estável)
 * ================================================================
 *    51 : 100kΩ EPCOS (pull-up 1 kΩ)
 *    52 : 200kΩ ATC Semitec 204GT-2 (pull-up 1 kΩ)
 *    55 : 100kΩ ATC Semitec 104GT-2 - usado em ParCan e J-Head (pull-up 1 kΩ)
 *
 * ================================================================
 *  Termistores analógicos - pull-up 10 kΩ - Atípico
 * ================================================================
 *    99 : 100kΩ Encontrado em algumas Wanhao i3 com resistor de pull-up de 10 kΩ
 *
 * ================================================================
 *  RTDs analógicos (Pt100/Pt1000)
 * ================================================================
 *   110 : Pt100  com pull-up 1 kΩ (atípico)
 *   147 : Pt100  com pull-up 4,7 kΩ
 *  1010 : Pt1000 com pull-up 1 kΩ (atípico)
 *  1022 : Pt1000 com pull-up 2,2 kΩ
 *  1047 : Pt1000 com pull-up 4,7 kΩ (E3D)
 *    20 : Pt100  com o circuito da Ultimainboard V2.x, com a tensão de referência do ADC da placa = tensão de alimentação da placa amplificadora INA826.
 *                OBS.: (1) Precisa usar uma entrada de ADC sem pull-up. (2) Alguns amplificadores INA826 são instáveis em 3,3V; considere os sensores 147, 110 ou 21.
 *    21 : Pt100  com o circuito da Ultimainboard V2.x, com referência de ADC de 3,3V (STM32, LPC176x....) e placa amplificadora INA826 alimentada em 5V.
 *                OBS.: os pinos de ADC não toleram 5V. Não recomendado, pois é possível danificar a CPU passando de 500°C.
 *   201 : Pt100  com o circuito da Overlord, semelhante à Ultimainboard V2.x
 *
 * ================================================================
 *  Placas SPI de RTD/termopar
 * ================================================================
 *    -5 : MAX31865 com Pt100/Pt1000, 2, 3 ou 4 fios  (só para os sensores 0-2 e a mesa)
 *                  OBS.: você precisa descomentar/ajustar os defines MAX31865_*_OHMS_n abaixo.
 *    -3 : MAX31855 com termopar, -200°C a +700°C (só para os sensores 0-2 e a mesa)
 *    -2 : MAX6675  com termopar, 0°C a +700°C    (só para os sensores 0-2 e a mesa)
 *
 *  OBS.: garanta que TEMP_n_CS_PIN esteja definido no seu arquivo de pinos para cada TEMP_SENSOR_n que use termopar SPI.
 *        Por padrão usa-se o SPI por hardware no barramento serial padrão. Se você também definir TEMP_n_SCK_PIN e
 *        TEMP_n_MISO_PIN, o SPI por software será usado nessas portas. É possível forçar o SPI por hardware no
 *        barramento padrão pelo arquivo Configuration_adv.h. Por ora, barramentos SPI por hardware separados para
 *        sensores não são suportados.
 *
 * ================================================================
 *  Placas analógicas de termopar
 * ================================================================
 *   -18 : ADS1118 com termopar, ex.: Mightyboard rev G/H
 *    -4 : AD8495 com termopar
 *    -1 : AD595  com termopar
 *
 * ================================================================
 *  Sensor interno do SoC
 * ================================================================
 *   100 : sensor interno do SoC
 *
 * ================================================================
 *  Sensores térmicos personalizados / fictícios / outros
 * ================================================================
 *     0 : não usado
 *  1000 : Personalizado - informe os parâmetros em Configuration_adv.h
 *
 *   !!! Use estes apenas para teste ou desenvolvimento. NUNCA numa máquina em produção. !!!
 *   998 : Tabela fictícia que SEMPRE lê 25°C ou a temperatura definida abaixo.
 *   999 : Tabela fictícia que SEMPRE lê 100°C ou a temperatura definida abaixo.
 */
#define TEMP_SENSOR_0 1
#define TEMP_SENSOR_1 0
#define TEMP_SENSOR_2 0
#define TEMP_SENSOR_3 0
#define TEMP_SENSOR_4 0
#define TEMP_SENSOR_5 0
#define TEMP_SENSOR_6 0
#define TEMP_SENSOR_7 0
#define TEMP_SENSOR_BED 1
#define TEMP_SENSOR_PROBE 0
#define TEMP_SENSOR_CHAMBER 0
#define TEMP_SENSOR_COOLER 0
#define TEMP_SENSOR_BOARD 0
#define TEMP_SENSOR_SOC 0
#define TEMP_SENSOR_REDUNDANT 0

// Leituras de temperatura constante dos termistores fictícios, para uso com 998 e 999
#define DUMMY_THERMISTOR_998_VALUE  25
#define DUMMY_THERMISTOR_999_VALUE 100

// Valores de resistor ao usar sensores MAX31865 (-5) em TEMP_SENSOR_0 / 1 / 2 / BED
#if TEMP_SENSOR_IS_MAX_TC(0)
  #define MAX31865_SENSOR_OHMS_0      100 // (Ω) Tipicamente 100 ou 1000 (PT100 ou PT1000)
  #define MAX31865_CALIBRATION_OHMS_0 430 // (Ω) Tipicamente 430 para o PT100 da Adafruit; 4300 para o PT1000 da Adafruit
#endif
#if TEMP_SENSOR_IS_MAX_TC(1)
  #define MAX31865_SENSOR_OHMS_1      100
  #define MAX31865_CALIBRATION_OHMS_1 430
#endif
#if TEMP_SENSOR_IS_MAX_TC(2)
  #define MAX31865_SENSOR_OHMS_2      100
  #define MAX31865_CALIBRATION_OHMS_2 430
#endif
#if TEMP_SENSOR_IS_MAX_TC(BED)
  #define MAX31865_SENSOR_OHMS_BED      100
  #define MAX31865_CALIBRATION_OHMS_BED 430
#endif

#if HAS_E_TEMP_SENSOR
  #define TEMP_RESIDENCY_TIME         10  // (segundos) Tempo de espera para o hotend "assentar" no M109
  #define TEMP_WINDOW                  1  // (°C) Proximidade de temperatura para o temporizador de "temperatura atingida"
  #define TEMP_HYSTERESIS              3  // (°C) Proximidade de temperatura considerada "perto o bastante" do alvo
#endif

#if TEMP_SENSOR_BED
  #define TEMP_BED_RESIDENCY_TIME     10  // (segundos) Tempo de espera para a mesa "assentar" no M190
  #define TEMP_BED_WINDOW              1  // (°C) Proximidade de temperatura para o temporizador de "temperatura atingida"
  #define TEMP_BED_HYSTERESIS          3  // (°C) Proximidade de temperatura considerada "perto o bastante" do alvo
#endif

#if TEMP_SENSOR_CHAMBER
  #define TEMP_CHAMBER_RESIDENCY_TIME 10  // (segundos) Tempo de espera para a câmara "assentar" no M191
  #define TEMP_CHAMBER_WINDOW          1  // (°C) Proximidade de temperatura para o temporizador de "temperatura atingida"
  #define TEMP_CHAMBER_HYSTERESIS      3  // (°C) Proximidade de temperatura considerada "perto o bastante" do alvo
#endif

/**
 * Sensor de temperatura redundante (TEMP_SENSOR_REDUNDANT)
 *
 * Usa um sensor de temperatura como redundância de outra leitura. Escolha um sensor de temperatura não utilizado
 * e outro sensor para o qual ele servirá de redundância. Se os dois termistores divergirem mais que
 * TEMP_SENSOR_REDUNDANT_MAX_DIFF (°C), a impressão será abortada. O sensor escolhido como redundante tem suas
 * funções normais desabilitadas; ou seja, escolher o sensor da mesa (-1) desativa o aquecimento/monitoramento dela.
 *
 * Para escolher origem/destino use: COOLER, PROBE, BOARD, CHAMBER, BED, E0, E1, E2, E3, E4, E5, E6, E7
 */
#if TEMP_SENSOR_REDUNDANT
  #define TEMP_SENSOR_REDUNDANT_SOURCE    E1  // O sensor que fornecerá a leitura redundante.
  #define TEMP_SENSOR_REDUNDANT_TARGET    E0  // O sensor para o qual estamos fornecendo a leitura redundante.
  #define TEMP_SENSOR_REDUNDANT_MAX_DIFF  10  // (°C) Diferença de temperatura que aborta a impressão.
#endif

// @section temperature

// Abaixo desta temperatura o aquecedor será desligado,
// porque isso provavelmente indica um fio de termistor rompido.
#define HEATER_0_MINTEMP   5
#define HEATER_1_MINTEMP   5
#define HEATER_2_MINTEMP   5
#define HEATER_3_MINTEMP   5
#define HEATER_4_MINTEMP   5
#define HEATER_5_MINTEMP   5
#define HEATER_6_MINTEMP   5
#define HEATER_7_MINTEMP   5
#define BED_MINTEMP        5
#define CHAMBER_MINTEMP    5

// Acima desta temperatura o aquecedor será desligado.
// Isso protege componentes de superaquecer, mas NÃO protege de curtos e falhas.
// (Use o MINTEMP para proteção contra curto/falha do termistor.)
#define HEATER_0_MAXTEMP 275
#define HEATER_1_MAXTEMP 275
#define HEATER_2_MAXTEMP 275
#define HEATER_3_MAXTEMP 275
#define HEATER_4_MAXTEMP 275
#define HEATER_5_MAXTEMP 275
#define HEATER_6_MAXTEMP 275
#define HEATER_7_MAXTEMP 275
#define BED_MAXTEMP      150
#define CHAMBER_MAXTEMP  60

/**
 * Ultrapassagem térmica (overshoot)
 * Durante o aquecimento (e a impressão) a temperatura costuma "ultrapassar" o alvo em vários graus
 * (principalmente antes do ajuste de PID). Definir o alvo perto demais do MAXTEMP garante um
 * desligamento por MAXTEMP! Use estes valores para impedir temperaturas perto demais do MAXTEMP.
 */
#define HOTEND_OVERSHOOT 15   // (°C) Proíbe temperaturas acima de MAXTEMP - OVERSHOOT
#define BED_OVERSHOOT    10   // (°C) Proíbe temperaturas acima de MAXTEMP - OVERSHOOT
#define COOLER_OVERSHOOT  2   // (°C) Proíbe temperaturas mais próximas que OVERSHOOT

//===========================================================================
//============================== Ajustes de PID =============================
//===========================================================================

// @section hotend temp

/**
 * Controle de temperatura
 *
 *  (NENHUM) : aquecimento liga-desliga (bang-bang)
 * PIDTEMP   : controle de temperatura por PID (~4,1K)
 * MPCTEMP   : controle de temperatura por modelo preditivo. (~1,8K sem auto-ajuste)
 */
#define PIDTEMP           // Veja o guia de ajuste de PID em https://reprap.org/wiki/PID_Tuning
//#define MPCTEMP         // Veja https://marlinfw.org/docs/features/model_predictive_control.html

#define PID_MAX  255      // Limita a corrente do hotend enquanto o PID está ativo (veja PID_FUNCTIONAL_RANGE abaixo); 255=corrente total
#define PID_K1     0.95   // Fator de suavização dentro de qualquer laço de PID

#if ENABLED(PIDTEMP)
  //#define MIN_POWER 0           // Potência mínima para melhorar a estabilidade do PID (0..PID_MAX).
                                  // Pegue a potência no relatório de temperatura ('M105' => @:nnn) e tente de P*2-20 a P*2-10.
  //#define PID_DEBUG             // Envia dados de depuração do PID pela serial. Use 'M303 D' para ligar/desligar.
  //#define PID_PARAMS_PER_HOTEND // Usa parâmetros de PID separados para cada extrusora (útil para extrusoras diferentes entre si)
                                  // Defina/consulte pelo G-code: M301 E[número da extrusora, 0-2]

  #if ENABLED(PID_PARAMS_PER_HOTEND)
    // Informe aqui até um valor por hotend, conforme a sua montagem.
    // Se houver menos valores, o último vale para os hotends restantes.
    #define DEFAULT_KP_LIST {  22.20,  22.20 }
    #define DEFAULT_KI_LIST {   1.08,   1.08 }
    #define DEFAULT_KD_LIST { 114.00, 114.00 }
  #else
    #define DEFAULT_KP  22.20
    #define DEFAULT_KI   1.08
    #define DEFAULT_KD 114.00
  #endif
#else
  #define BANG_MAX 255    // Limita a corrente do hotend no modo liga-desliga; 255=corrente total
#endif

/**
 * Controle preditivo por modelo (MPC) para o hotend
 *
 * Usa um modelo físico do hotend para controlar a temperatura. Bem configurado, isso dá
 * resposta e estabilidade melhores que o PID e dispensa PID_EXTRUSION_SCALING
 * e PID_FAN_SCALING. Habilite MPC_AUTOTUNE e use M306 T para auto-ajustar o modelo.
 * @section mpc temp
 */
#if ENABLED(MPCTEMP)
  #define MPC_AUTOTUNE                                // Inclui um método de auto-ajuste do MPC (~6,3K bytes de flash)
  #if ENABLED(MPC_AUTOTUNE)
    //#define MPC_AUTOTUNE_DEBUG                      // Habilita o log de depuração do MPC (~870 bytes de flash)
  #endif
  //#define MPC_EDIT_MENU                             // Adiciona a edição do MPC ao menu "Ajustes avançados". (~1,3K bytes de flash)
  //#define MPC_AUTOTUNE_MENU                         // Adiciona o auto-ajuste do MPC ao menu "Ajustes avançados". (~350 bytes de flash)

  #define MPC_MAX 255                                 // (0..255) Corrente para o bico enquanto o MPC está ativo.
  #define MPC_HEATER_POWER { 40.0f }                  // (W) Potências nominais dos cartuchos aquecedores.
  //#define MPC_PTC                                   // A potência do hotend varia com a temperatura (ex.: cartuchos PTC).
  #if ENABLED(MPC_PTC)
    #define MPC_HEATER_ALPHA { 0.0028f }              // Coeficiente de temperatura da resistência dos cartuchos aquecedores.
    #define MPC_HEATER_REFTEMP { 20 }                 // (°C) Temperatura de referência para MPC_HEATER_POWER e MPC_HEATER_ALPHA.
  #endif

  #define MPC_INCLUDE_FAN                             // Modelar a velocidade da ventoinha?

  // Constantes físicas medidas pelo M306
  #define MPC_BLOCK_HEAT_CAPACITY { 16.7f }           // (J/K) Capacidades térmicas dos blocos aquecedores.
  #define MPC_SENSOR_RESPONSIVENESS { 0.22f }         // (K/s por ∆K) Taxa de variação da temperatura do sensor a partir do bloco aquecedor.
  #define MPC_AMBIENT_XFER_COEFF { 0.068f }           // (W/K) Coeficientes de troca de calor do bloco para o ar ambiente, com a ventoinha desligada.
  #if ENABLED(MPC_INCLUDE_FAN)
    #define MPC_AMBIENT_XFER_COEFF_FAN255 { 0.097f }  // (W/K) Coeficientes de troca de calor do bloco para o ar ambiente, com a ventoinha no máximo.
  #endif

  // Com uma ventoinha e vários hotends, o MPC precisa saber como aplicar o efeito de resfriamento da ventoinha.
  #if ENABLED(MPC_INCLUDE_FAN)
    //#define MPC_FAN_0_ALL_HOTENDS
    //#define MPC_FAN_0_ACTIVE_HOTEND
  #endif

  // Capacidade térmica do filamento (joules/kelvin/mm)
  // Ajustável em tempo de execução com M306 H<valor>
  #define FILAMENT_HEAT_CAPACITY_PERMM { 5.6e-3f }    // 0,0056 J/K/mm para PLA 1,75mm (0,0149 J/K/mm para PLA 2,85mm).
                                                      // 0,0036 J/K/mm para PETG 1,75mm (0,0094 J/K/mm para PETG 2,85mm).
                                                      // 0,00515 J/K/mm para ABS 1,75mm (0,0137 J/K/mm para ABS 2,85mm).
                                                      // 0,00522 J/K/mm para Nylon 1,75mm (0,0138 J/K/mm para Nylon 2,85mm).

  // Opções avançadas
  #define MPC_SMOOTHING_FACTOR 0.5f                   // (0.0...1.0) Sensores de temperatura ruidosos podem precisar de um valor menor para estabilizar.
  #define MPC_MIN_AMBIENT_CHANGE 1.0f                 // (K/s) Taxa de variação modelada da temperatura ambiente, ao corrigir imprecisões do modelo.
  #define MPC_STEADYSTATE 0.5f                        // (K/s) Taxa de variação de temperatura para a lógica de regime permanente ser aplicada.

  #define MPC_TUNING_POS { X_CENTER, Y_CENTER, 1.0f } // (mm) Posição de auto-ajuste do M306, idealmente o centro da mesa na altura da primeira camada.
  #define MPC_TUNING_END_Z 10.0f                      // (mm) Posição Z final do auto-ajuste do M306.
  //#define EVENT_GCODE_AFTER_MPC_TUNE "M84"          // G-code a executar depois que o ajuste do MPC terminar e o Z subir.
#endif

//===========================================================================
//================= PID > Controle de temperatura da mesa ===================
//===========================================================================

// @section bed temp

/**
 * Potência máxima da mesa
 * Vale para todas as formas de controle da mesa (PID, liga-desliga e liga-desliga com histerese).
 * Com qualquer valor abaixo de 255, habilita uma forma de PWM na mesa que funciona como um divisor,
 * então só use se você aceitar PWM na sua mesa. (Veja o comentário sobre habilitar o PIDTEMPBED)
 */
#define MAX_BED_POWER 255 // limita o ciclo de trabalho da mesa; 255=corrente total

/**
 * Aquecimento da mesa por PID
 *
 * A frequência do PID será a mesma do PWM da extrusora.
 * Se PID_dT estiver no padrão e correto para o hardware/configuração, isso significa 7,689Hz,
 * que é adequado para acionar uma onda quadrada numa carga resistiva e não aquece o FET de forma
 * significativa. Também funciona bem num relé de estado sólido Fotek SSR-10DA com um aquecedor de
 * 250W. Se a sua configuração for bem diferente disso e você não entender os riscos envolvidos,
 * não use PID na mesa até que alguém verifique que o seu hardware funciona.
 *
 * Com esta opção desabilitada, será usado o modo liga-desliga. BED_LIMIT_SWITCHING habilita a histerese.
 */
//#define PIDTEMPBED

#if ENABLED(PIDTEMPBED)
  //#define MIN_BED_POWER 0   // Potência mínima para melhorar a estabilidade do PID (0..MAX_BED_POWER).
                              // Pegue a potência no relatório de temperatura ('M105' => B@:nnn) e tente de P*2-20 a P*2-10.
  //#define PID_BED_DEBUG     // Envia dados de depuração do PID da mesa pela serial. Use 'M303 D' para ligar/desligar.

  // Aquecedor de silicone 120V 250W sobre borossilicato de 4mm (MendelMax 1.5+)
  // a partir do modelo FOPDT - kp=.39 Tp=405 Tdead=66, Tc ajustado para 79.2, fator agressivo de .15 (vs .1, 1, 10)
  #define DEFAULT_BED_KP  10.00
  #define DEFAULT_BED_KI   0.023
  #define DEFAULT_BED_KD 305.4

  // ACHE OS SEUS: "M303 E-1 C8 S90" roda o auto-ajuste na mesa a 90 graus C por 8 ciclos.
#else
  //#define BED_LIMIT_SWITCHING   // Mantém a temperatura da mesa dentro de BED_LIMIT_HYSTERESIS do alvo
#endif

/**
 * Mesa Peltier - aquecimento e resfriamento
 *
 * Um dispositivo Peltier transfere calor de um lado para o outro em proporção à corrente que
 * passa por ele e ao sentido dessa corrente. Ou seja, o mesmo dispositivo tanto aquece quanto
 * resfria.
 *
 * Ao "resfriar", além de dissipar o calor transferido do lado quente para o lado frio, também é
 * preciso dissipar a potência consumida (tensão * corrente). Garanta que exista uma ventoinha
 * que possa ser alimentada em sincronia com o módulo Peltier.
 *
 * Este recurso só funciona em modo liga-desliga, porque Peltiers não lidam bem com PWM
 * sem circuito de filtragem.
 *
 * Como as impressoras 3D existentes já são feitas para suportar corrente relativamente alta na
 * mesa aquecida, podemos usar os pinos de potência da mesa para controlar o Peltier com os
 * mesmos G-codes da mesa aquecida (M140, M190, etc.).
 *
 * Um segundo pino GPIO é necessário para controlar o sentido da corrente.
 * Duas configurações são possíveis: relé e ponte H.
 *
 * (Por enquanto só o relé é suportado. A ponte H exige 4 chaves MOS em configuração de ponte H.)
 *
 * A potência é tratada pelo laço de controle liga-desliga: 0 ou 255.
 * Aplicações de resfriamento são mais comuns que as de aquecimento, então os estados do pino
 * costumam ser:
 *   LOW  = Aquecendo = relé energizado
 *   HIGH = Resfriando = relé no estado "normal"
 */
//#define PELTIER_BED
#if ENABLED(PELTIER_BED)
  #define PELTIER_DIR_PIN           -1  // Pino de controle do relé do Peltier
  #define PELTIER_DIR_HEAT_STATE   LOW  // Estado do pino do relé que faz o Peltier aquecer
#endif

// Adiciona 'M190 R T' para um resfriamento mais gradual da mesa com M190 R.
//#define BED_ANNEALING_GCODE

//===========================================================================
//================ PID > Controle de temperatura da câmara ==================
//===========================================================================

/**
 * Aquecimento da câmara por PID
 *
 * Se esta opção estiver habilitada, ajuste as constantes de PID abaixo.
 * Se estiver desabilitada, será usado o modo liga-desliga e CHAMBER_LIMIT_SWITCHING habilitará
 * a histerese.
 *
 * A frequência do PID será a mesma do PWM da extrusora.
 * Se PID_dT estiver no padrão e correto para o hardware/configuração, isso significa 7,689Hz,
 * que é adequado para acionar uma onda quadrada numa carga resistiva e não aquece o FET de forma
 * significativa. Também funciona bem num relé de estado sólido Fotek SSR-10DA com um aquecedor de
 * 200W. Se a sua configuração for bem diferente disso e você não entender os riscos envolvidos,
 * não use PID na câmara até que alguém verifique que o seu hardware funciona.
 * @section chamber temp
 */
//#define PIDTEMPCHAMBER
//#define CHAMBER_LIMIT_SWITCHING

/**
 * Potência máxima da câmara
 * Vale para todas as formas de controle da câmara (PID, liga-desliga e liga-desliga com histerese).
 * Com qualquer valor abaixo de 255, habilita uma forma de PWM no aquecedor da câmara que funciona
 * como um divisor, então só use se você aceitar PWM no seu aquecedor. (Veja o comentário sobre
 * habilitar o PIDTEMPCHAMBER)
 */
#define MAX_CHAMBER_POWER 255 // limita o ciclo de trabalho do aquecedor da câmara; 255=corrente total

#if ENABLED(PIDTEMPCHAMBER)
  //#define MIN_CHAMBER_POWER 0 // Potência mínima para melhorar a estabilidade do PID. (0..MAX_CHAMBER_POWER)
                                // Pegue a potência no relatório de temperatura ('M105' => C@:nnn) e tente de P*2-20 a P*2-10.
  //#define PID_CHAMBER_DEBUG   // Envia dados de depuração do PID da câmara pela serial. Use 'M303 D' para ligar/desligar.

  // Lasko "MyHeat Personal Heater" (200W) modificado com um Fotek SSR-10DA para controlar só o elemento
  // de aquecimento, colocado dentro da pequena tenda de gabinete da Creality.
  #define DEFAULT_CHAMBER_KP  37.04
  #define DEFAULT_CHAMBER_KI   1.40
  #define DEFAULT_CHAMBER_KD 655.17
  // M309 P37.04 I1.04 D655.17

  // ACHE OS SEUS: "M303 E-2 C8 S50" roda o auto-ajuste na câmara a 50 graus C por 8 ciclos.
#endif // PIDTEMPCHAMBER

// @section pid temp

#if ANY(PIDTEMP, PIDTEMPBED, PIDTEMPCHAMBER)
  //#define PID_OPENLOOP          // Coloca o PID em malha aberta. M104/M140 define a potência de saída de 0 a PID_MAX
  //#define SLOW_PWM_HEATERS      // PWM de frequência muito baixa (cerca de 0,125Hz=8s) e tempo mínimo de estado de aproximadamente 1s, útil para aquecedores acionados por relé
  #define PID_FUNCTIONAL_RANGE 20 // Se a diferença entre a temperatura alvo e a temperatura real
                                  // for maior que PID_FUNCTIONAL_RANGE, o PID será desligado e o aquecedor irá para mín/máx.

  //#define PID_EDIT_MENU         // Adiciona a edição do PID ao menu "Ajustes avançados". (~700 bytes de flash)
  //#define PID_AUTOTUNE_MENU     // Adiciona o auto-ajuste do PID ao menu "Ajustes avançados". (~250 bytes de flash)
#endif

// @section safety

/**
 * Impede a extrusão se a temperatura estiver abaixo de EXTRUDE_MINTEMP.
 * Adiciona o M302 para definir a temperatura mínima de extrusão e/ou
 * ligar e desligar a proteção contra extrusão a frio.
 *
 * *** É ALTAMENTE RECOMENDADO DEIXAR ESTA OPÇÃO HABILITADA! ***
 */
#define PREVENT_COLD_EXTRUSION
#define EXTRUDE_MINTEMP 170

/**
 * Impede uma extrusão única maior que EXTRUDE_MAXLENGTH.
 * Obs.: em extrusoras Bowden, use um valor grande o bastante para permitir carga/descarga.
 */
#define PREVENT_LENGTHY_EXTRUDE
#define EXTRUDE_MAXLENGTH 200

//===========================================================================
//=================== Proteção contra disparada térmica =====================
//===========================================================================

/**
 * A proteção térmica dá à sua impressora uma camada extra de proteção contra danos
 * e incêndio. O Marlin sempre inclui faixas seguras de temperatura mínima e máxima,
 * que protegem contra um fio de termistor rompido ou desconectado.
 *
 * O problema: se um termistor se soltar, ele vai reportar a temperatura bem mais baixa
 * do ar do ambiente, e o firmware vai manter o aquecedor ligado.
 *
 * Se você receber erros de "Thermal Runaway" ou "Heating failed", os
 * detalhes podem ser ajustados em Configuration_adv.h
 */

#define THERMAL_PROTECTION_HOTENDS // Habilita a proteção térmica de todas as extrusoras
#define THERMAL_PROTECTION_BED     // Habilita a proteção térmica da mesa aquecida
#define THERMAL_PROTECTION_CHAMBER // Habilita a proteção térmica da câmara aquecida
#define THERMAL_PROTECTION_COOLER  // Habilita a proteção térmica do resfriamento do laser

//===========================================================================
//========================= Ajustes mecânicos ===============================
//===========================================================================

// @section kinematics

// Habilite uma das opções abaixo para cinemática CoreXY, CoreXZ ou CoreYZ,
// seja na ordem usual ou invertida
#define COREXY
//#define COREXZ
//#define COREYZ
//#define COREYX
//#define COREZX
//#define COREZY
#endif

// Habilite para uma impressora de esteira com movimento "Z" infinito
//#define BELTPRINTER

// Robô articulado (braço). As juntas são mapeadas diretamente nos eixos, sem cinemática.
//#define ARTICULATED_ROBOT_ARM

// Para um cortador de fio quente com eixos horizontais paralelos (X, I), em que as alturas das duas
// pontas do fio são controladas por eixos paralelos (Y, J). As juntas são mapeadas diretamente nos eixos (sem cinemática).
//#define FOAMCUTTER_XYUV

// @section polargraph

/**
 * Cinemática Polargraph
 */
//#define POLARGRAPH
#if ENABLED(POLARGRAPH)
  #define POLARGRAPH_MAX_BELT_LEN  1035.0 // (mm) Comprimento da correia totalmente estendida. Sobreponha com M665 H.
  #define DEFAULT_SEGMENTS_PER_SECOND 5   // Segmentação do movimento baseada na duração
  #define PEN_UP_DOWN_MENU                // Adiciona "Caneta acima" e "Caneta abaixo" ao menu do MarlinUI
#endif

// @section delta

/**
 * Cinemática DELTA
 */
//#define DELTA
#if ENABLED(DELTA)

  // Monta as curvas da delta a partir de muitas retas (interpolação linear).
  // É um meio-termo entre cantos visíveis (poucos segmentos)
  // e sobrecarga do processador (chamadas sqrt demais, que são caras).
  #define DEFAULT_SEGMENTS_PER_SECOND 200

  // Depois do homing, desce até uma altura em que o movimento XY fica livre
  //#define DELTA_HOME_TO_SAFE_ZONE

  // Menu de calibração da delta
  // Adiciona a calibração de três pontos ao menu do MarlinUI.
  // Veja http://minow.blogspot.com/index.html#4918805519571907051
  //#define DELTA_CALIBRATION_MENU

  // Autocalibração G33 da delta. Habilite EEPROM_SETTINGS para gravar os resultados.
  //#define DELTA_AUTO_CALIBRATION

  #if ENABLED(DELTA_AUTO_CALIBRATION)
    // Número padrão de pontos de sondagem : n*n (1 -> 7)
    #define DELTA_CALIBRATION_DEFAULT_POINTS 4
  #endif

  #if ANY(DELTA_AUTO_CALIBRATION, DELTA_CALIBRATION_MENU)
    // Tamanho do passo na sondagem pelo teste do papel
    #define PROBE_MANUALLY_STEP 0.05      // (mm)
  #endif

  // Diâmetro/2 da superfície de impressão menos o espaço inalcançável (evita colisões com as torres verticais).
  #define PRINTABLE_RADIUS       140.0    // (mm)

  // Distância entre centros dos furos das hastes diagonais.
  #define DELTA_DIAGONAL_ROD 250.0        // (mm)

  // Distância entre a mesa e a posição de home Z do bico
  #define DELTA_HEIGHT 250.00             // (mm) Obtenha este valor com a autocalibração G33

  #define DELTA_ENDSTOP_ADJ { 0.0, 0.0, 0.0 } // (mm) Obtenha estes valores com a autocalibração G33

  // Distância horizontal vencida pelas hastes diagonais com o efetor centralizado.
  #define DELTA_RADIUS 124.0              // (mm) Obtenha este valor com a autocalibração G33

  // Ajustes finos de cada torre
  // correções de ângulo das torres X e Y / rotaciona XYZ de modo que o ângulo da torre Z = 0
  // medido em graus no sentido anti-horário, olhando a impressora de cima
  #define DELTA_TOWER_ANGLE_TRIM { 0.0, 0.0, 0.0 } // (mm) Obtenha estes valores com a autocalibração G33

  // Ajustes do raio da delta e das hastes diagonais
  //#define DELTA_RADIUS_TRIM_TOWER       { 0.0, 0.0, 0.0 } // (mm)
  //#define DELTA_DIAGONAL_ROD_TRIM_TOWER { 0.0, 0.0, 0.0 } // (mm)

#endif // DELTA

// @section scara

/**
 * Cinemática SCARA
 *
 * - Ângulos positivos giram no sentido anti-horário, olhando de cima.
 * - Theta é o ângulo do ombro. 0 grau se alinha ao X cartesiano positivo.
 * - Psi é o ângulo do cotovelo. 0° é reto, ±180° é totalmente dobrado (destro positivo, canhoto negativo).
 *
 * A Morgan SCARA foi desenvolvida por QHARLEY na África do Sul em 2012-2013.
 * Implementada e levemente retrabalhada por JCERNY em junho de 2014.
 *
 * A Mostly Printed SCARA é um projeto de código aberto de Tyler Williams. Veja:
 *   https://www.thingiverse.com/thing:2487048
 *   https://www.thingiverse.com/thing:1241491
 */

//#define SCARA
#if ENABLED(SCARA)
  // Se o movimento ficar truncado, tente reduzir este valor
  #define DEFAULT_SEGMENTS_PER_SECOND 200

  // Comprimento dos braços de apoio interno e externo. Meça os braços com precisão.
  #define SCARA_LINKAGE_1    135    // (mm)
  #define SCARA_LINKAGE_2    135    // (mm)

  // Offset da torre SCARA (posição do eixo do ombro em relação à posição zero da mesa)
  // Precisa ser razoavelmente preciso, pois define a posição da mesa no espaço SCARA.
  #define SCARA_OFFSET_X       0    // (mm)
  #define SCARA_OFFSET_Y    -150    // (mm)

  // Raio da área inalcançável perto do eixo do ombro
  #define MIDDLE_DEAD_ZONE_R   0    // (mm)

  // Sentido de dobra do cotovelo. 1 = destro (anti-horário), -1 = canhoto (horário)
  #define SCARA_ELBOW_DIR 1

  // Define como o movimento do ombro afeta o ângulo do braço distal. Valores comuns:
  // 0.0 se o braço distal mantém o ângulo em relação ao braço proximal (ex.: se o motor do cotovelo viaja no braço proximal)
  // 1.0 se o braço distal mantém o ângulo em relação ao eixo X cartesiano (ex.: Morgan e MPSCARA)
  // Uma redução em dois estágios com uma polia intermediária no eixo do ombro tem diafonia =
  //   dentes_da_polia_intermediária/dentes_da_polia_do_cotovelo (o primeiro estágio de redução não afeta a diafonia)
  #define SCARA_CROSSTALK_FACTOR (40.0/60.0)

  // Ângulos de ombro e cotovelo na posição de home. Se não for definido, usa-se o home cartesiano e
  // os ângulos são calculados por cinemática inversa (obs.: os offsets de home do M665 continuam sendo ângulos)
  #define SCARA_HOME_THETA   -40
  #define SCARA_HOME_PSI     160

  // Habilita M360-M364 para calibrar os ângulos da SCARA
  //#define SCARA_CALIBRATION

#endif // SCARA

// @section tpara

// Habilite para cinemática TPARA e configure abaixo
//#define AXEL_TPARA
#if ENABLED(AXEL_TPARA)
  #define DEBUG_TPARA_KINEMATICS
  #define DEFAULT_SEGMENTS_PER_SECOND 200

  // Comprimento dos braços de apoio interno e externo. Meça os braços com precisão.
  #define TPARA_LINKAGE_1 120     // (mm)
  #define TPARA_LINKAGE_2 120     // (mm)

  // Altura do eixo do ombro (pivô) em relação ao piso da torre
  #define TPARA_SHOULDER_AXIS_HEIGHT 135.0     // (mm)

  // Posição da última articulação em relação à origem do braço robô
  // (interseção do eixo da base com o piso) quando na posição de home
  #define TPARA_ARM_X_HOME_POS  28.75  // (mm) Medido do eixo do ombro ao eixo do porta-ferramenta na posição de home
  #define TPARA_ARM_Y_HOME_POS   0     // (mm)
  #define TPARA_ARM_Z_HOME_POS 250.00  // (mm) Medido do eixo do porta-ferramenta até o piso

  // Offset do espaço de trabalho TPARA em relação à torre (posição da origem do espaço de trabalho em relação à origem da torre do robô)
  // Precisa ser razoavelmente preciso, pois define a posição da mesa no espaço TPARA.
  #define TPARA_OFFSET_X    127.0     // (mm) para coincidir com o raio mínimo MIDDLE_DEAD_ZONE_R, e W(0,0,0) ser alcançável
  #define TPARA_OFFSET_Y      0.0     // (mm)
  #define TPARA_OFFSET_Z      0.0     // (mm)

  // Offset do ponto de conexão da ferramenta TPARA, relativo à origem do referencial móvel da ferramenta, que fica no eixo
  // da última articulação (TCP: ponto central/de conexão da ferramenta) do robô.
  // O plano do offset medido precisa estar alinhado com o plano da posição de home
  #define TPARA_TCP_OFFSET_X    27.0     // (mm) Flange da ferramenta: 27 (distância do pivô aos furos dos parafusos), ferramenta extrusora: 50.0,
  #define TPARA_TCP_OFFSET_Y     0.0     // (mm)
  #define TPARA_TCP_OFFSET_Z   -65.0     // (mm) Flange da ferramenta (base): -6 (cuidado: a posição Z 0 fará a segunda articulação bater no piso; -35 é seguro para testes sem ferramenta), ferramenta extrusora (depende da extrusora): -65.0

  #define FEEDRATE_SCALING        // Converte a velocidade XY de mm/s para graus/s em tempo real

  // Raio ao redor do centro em que o braço não alcança
  // Por ora usa-se um limite uniforme fixo, embora devesse ser calculado, ou fixar um limite para o ângulo de cada eixo
  #define MIDDLE_DEAD_ZONE_R   100    // (mm)

  // Ângulo máximo entre L1 e L2
  #define TPARA_MAX_L1L2_ANGLE 140.0f // (graus)
#endif // AXEL_TPARA

// @section polar

/**
 * Cinemática POLAR
 *  desenvolvida por Kadir ilkimen para a PolarBear CNC e a babyBear
 *  https://github.com/kadirilkimen/Polar-Bear-Cnc-Machine
 *  https://github.com/kadirilkimen/babyBear-3D-printer
 *
 * Uma máquina polar pode ter configurações diferentes.
 * Esta cinemática só é compatível com a seguinte configuração:
 *        X : linear independente
 *   Y ou B : polar
 *        Z : linear independente
 *
 * Por exemplo, a PolarBear tem CoreXZ mais Y ou B polar.
 *
 * Problema de movimento do eixo polar perto do centro / origem:
 *
 * Impressão 3D:
 * Movimentos muito próximos do centro do eixo polar levam mais tempo que os demais.
 * Esse pequeno atraso resulta em mais material depositado, por causa da pressão no bico.
 *
 * A cinemática atual e o escalonamento de velocidade lidam com isso fazendo o movimento o mais
 * rápido possível. Funciona para movimentos lentos, mas não funciona bem com os rápidos. Uma
 * compensação de extrusão mais elaborada precisa ser implementada.
 *
 * No ideal, deveria prever que uma rotação longa perto do centro está por vir e causará
 * deposição indesejada. Assim poderia compensar a extrusão antecipadamente.
 *
 * Corte a laser:
 * O mesmo seria um problema para gravação a laser. Como o equipamento passa tempo girando no
 * ponto central, é provável que queime mais material do que deveria. Portanto, uma compensação
 * parecida seria implementada para operações de corte a laser.
 *
 * Fresamento:
 * Isso não deve ser um problema em operações de corte/fresamento.
 */
//#define POLAR
#if ENABLED(POLAR)
  #define DEFAULT_SEGMENTS_PER_SECOND 180   // Se o movimento ficar truncado, tente reduzir este valor
  #define PRINTABLE_RADIUS 82.0f            // (mm) Curso máximo do eixo X

  // Movimentos que caem dentro de POLAR_FAST_RADIUS recebem a maior velocidade possível,
  // para compensar a deposição indesejada do problema de movimento perto da origem.
  #define POLAR_FAST_RADIUS 3.0f            // (mm)

  // Raio inalcançável pela ferramenta.
  // Necessário se a ferramenta não estiver perfeitamente alinhada ao centro do eixo polar.
  #define POLAR_CENTER_OFFSET 0.0f          // (mm)

  #define FEEDRATE_SCALING                  // Converte a velocidade XY de mm/s para graus/s em tempo real
#endif

//===========================================================================
//===================== Ajustes dos fins de curso ===========================
//===========================================================================

// @section endstops

// Habilita o pull-up de todos os fins de curso, para evitar estado flutuante
#define ENDSTOPPULLUPS
#if DISABLED(ENDSTOPPULLUPS)
  // Desabilite ENDSTOPPULLUPS para definir os pull-ups individualmente
  //#define ENDSTOPPULLUP_XMIN
  //#define ENDSTOPPULLUP_YMIN
  //#define ENDSTOPPULLUP_ZMIN
  //#define ENDSTOPPULLUP_IMIN
  //#define ENDSTOPPULLUP_JMIN
  //#define ENDSTOPPULLUP_KMIN
  //#define ENDSTOPPULLUP_UMIN
  //#define ENDSTOPPULLUP_VMIN
  //#define ENDSTOPPULLUP_WMIN
  //#define ENDSTOPPULLUP_XMAX
  //#define ENDSTOPPULLUP_YMAX
  //#define ENDSTOPPULLUP_ZMAX
  //#define ENDSTOPPULLUP_IMAX
  //#define ENDSTOPPULLUP_JMAX
  //#define ENDSTOPPULLUP_KMAX
  //#define ENDSTOPPULLUP_UMAX
  //#define ENDSTOPPULLUP_VMAX
  //#define ENDSTOPPULLUP_WMAX
  //#define ENDSTOPPULLUP_ZMIN_PROBE
#endif

// Habilita o pull-down de todos os fins de curso, para evitar estado flutuante
//#define ENDSTOPPULLDOWNS
#if DISABLED(ENDSTOPPULLDOWNS)
  // Desabilite ENDSTOPPULLDOWNS para definir os pull-downs individualmente
  //#define ENDSTOPPULLDOWN_XMIN
  //#define ENDSTOPPULLDOWN_YMIN
  //#define ENDSTOPPULLDOWN_ZMIN
  //#define ENDSTOPPULLDOWN_IMIN
  //#define ENDSTOPPULLDOWN_JMIN
  //#define ENDSTOPPULLDOWN_KMIN
  //#define ENDSTOPPULLDOWN_UMIN
  //#define ENDSTOPPULLDOWN_VMIN
  //#define ENDSTOPPULLDOWN_WMIN
  //#define ENDSTOPPULLDOWN_XMAX
  //#define ENDSTOPPULLDOWN_YMAX
  //#define ENDSTOPPULLDOWN_ZMAX
  //#define ENDSTOPPULLDOWN_IMAX
  //#define ENDSTOPPULLDOWN_JMAX
  //#define ENDSTOPPULLDOWN_KMAX
  //#define ENDSTOPPULLDOWN_UMAX
  //#define ENDSTOPPULLDOWN_VMAX
  //#define ENDSTOPPULLDOWN_WMAX
  //#define ENDSTOPPULLDOWN_ZMIN_PROBE
#endif

/**
 * Fiação dos fins de curso (apenas os 3 conectores da placa, todos NF):
 *   X -> X_MIN em série com X_MAX  (home no mínimo)
 *   Y -> Y_MIN em série com Y_MAX  (home no mínimo)
 *   Z -> Z_MIN em série com Z_MAX  (home no mínimo, Z_MIN = camada 0)
 *
 * Em ligação série NF, qualquer sensor que abrir dispara o mesmo sinal. A
 * distinção é feita pelo sentido do movimento: no homing só o sensor daquele
 * extremo pode ser alcançado.
 *
 * Para separar o sensor MAX do eixo Z num pino próprio (e assim saber qual dos
 * dois disparou), descomente a linha abaixo e ligue o Z_MAX no EXP2_03:
 */
//#define Z_OTHR_PIN 14   // EXP2_03 - sensor Z MAX em pino separado

/**
 * Estado de "acionado" do fim de curso
 * Defina o estado (HIGH ou LOW) que vale para cada fim de curso.
 * Sensores NF (normalmente fechados) com pull-up: em repouso o circuito está
 * fechado e o pino lê LOW; ao acionar, o circuito abre e o pino vai para HIGH.
 */
#define X_MIN_ENDSTOP_HIT_STATE HIGH
#define X_MAX_ENDSTOP_HIT_STATE HIGH
#define Y_MIN_ENDSTOP_HIT_STATE HIGH
#define Y_MAX_ENDSTOP_HIT_STATE HIGH
#define Z_MIN_ENDSTOP_HIT_STATE HIGH
#define Z_MAX_ENDSTOP_HIT_STATE HIGH
#define I_MIN_ENDSTOP_HIT_STATE HIGH
#define I_MAX_ENDSTOP_HIT_STATE HIGH
#define J_MIN_ENDSTOP_HIT_STATE HIGH
#define J_MAX_ENDSTOP_HIT_STATE HIGH
#define K_MIN_ENDSTOP_HIT_STATE HIGH
#define K_MAX_ENDSTOP_HIT_STATE HIGH
#define U_MIN_ENDSTOP_HIT_STATE HIGH
#define U_MAX_ENDSTOP_HIT_STATE HIGH
#define V_MIN_ENDSTOP_HIT_STATE HIGH
#define V_MAX_ENDSTOP_HIT_STATE HIGH
#define W_MIN_ENDSTOP_HIT_STATE HIGH
#define W_MAX_ENDSTOP_HIT_STATE HIGH
#define Z_MIN_PROBE_ENDSTOP_HIT_STATE HIGH

// Habilite este recurso se todos os pinos de fim de curso usados suportarem interrupção.
// Isso elimina a necessidade de ficar consultando os pinos, economizando muitos ciclos de CPU.
//#define ENDSTOP_INTERRUPTS_FEATURE

/**
 * Limiar de ruído dos fins de curso
 *
 * Habilite se o seu probe ou fins de curso disparam falsamente por causa de ruído.
 *
 * - Valores maiores podem afetar a repetibilidade ou a precisão de alguns probes de mesa.
 * - Para resolver ruído, instale um capacitor cerâmico de 100nF em paralelo com a chave.
 * - Este recurso não é necessário para microchaves comuns montadas em placas
 *   baseadas no projeto da Makerbot, que já trazem o capacitor de 100nF.
 *
 * :[2,3,4,5,6,7]
 */
//#define ENDSTOP_NOISE_THRESHOLD 2

// Verifica se há fins de curso travados ou desconectados durante os movimentos de homing.
//#define DETECT_BROKEN_ENDSTOP

//=============================================================================
//========================= Ajustes de movimento ==============================
//=============================================================================
// @section motion

/**
 * Ajustes padrão
 *
 * Estes ajustes podem ser restaurados pelo M502
 *
 * Note que, se a EEPROM estiver habilitada, os valores salvos sobrepõem estes.
 */

/**
 * Com esta opção, cada motor E pode ter os seus próprios fatores para os
 * ajustes de movimento abaixo. Se forem informados menos fatores do que o
 * número total de extrusoras, o último valor vale para as restantes.
 */
//#define DISTINCT_E_FACTORS

/**
 * Passos por unidade padrão de cada eixo (linear=passos/mm, rotacional=passos/°)
 * Sobreponha com M92 (quando habilitado abaixo)
 *                                      X, Y, Z [, I [, J [, K...]]], E0 [, E1[, E2...]]
 */
#define DEFAULT_AXIS_STEPS_PER_UNIT   { 80, 80, 400, 415 }  // E0: BMG Two Trees dual drive (3:1) @1/16

/**
 * Habilita o suporte ao M92. Desabilite para economizar pelo menos ~530 bytes de flash.
 */
#define EDITABLE_STEPS_PER_UNIT

/**
 * Velocidade máxima padrão (linear=mm/s, rotacional=°/s)
 * Sobreponha com M203
 *                                      X, Y, Z [, I [, J [, K...]]], E0 [, E1[, E2...]]
 */
#define DEFAULT_MAX_FEEDRATE          { 300, 300, 5, 25 }

//#define LIMITED_MAX_FR_EDITING        // Limita a edição via M203 ou LCD a DEFAULT_MAX_FEEDRATE * 2
#if ENABLED(LIMITED_MAX_FR_EDITING)
  #define MAX_FEEDRATE_EDIT_VALUES    { 600, 600, 10, 50 } // ...ou defina os seus próprios limites de edição
#endif

/**
 * Aceleração máxima padrão (variação de velocidade no tempo) (linear=mm/(s^2), rotacional=°/(s^2))
 * (Velocidade inicial máxima para movimentos acelerados)
 * Sobreponha com M201
 *                                      X, Y, Z [, I [, J [, K...]]], E0 [, E1[, E2...]]
 */
#define DEFAULT_MAX_ACCELERATION      { 3000, 3000, 100, 10000 }

//#define LIMITED_MAX_ACCEL_EDITING     // Limita a edição via M201 ou LCD a DEFAULT_MAX_ACCELERATION * 2
#if ENABLED(LIMITED_MAX_ACCEL_EDITING)
  #define MAX_ACCEL_EDIT_VALUES       { 6000, 6000, 200, 20000 } // ...ou defina os seus próprios limites de edição
#endif

/**
 * Aceleração padrão (variação de velocidade no tempo) (linear=mm/(s^2), rotacional=°/(s^2))
 * Sobreponha com M204
 *
 *   M204 P    Aceleração de impressão
 *   M204 R    Aceleração de retração
 *   M204 T    Aceleração de deslocamento
 */
#define DEFAULT_ACCELERATION          3000    // Aceleração de X, Y, Z e E nos movimentos de impressão
#define DEFAULT_RETRACT_ACCELERATION  3000    // Aceleração do E nas retrações
#define DEFAULT_TRAVEL_ACCELERATION   3000    // Aceleração de X, Y, Z nos deslocamentos (sem imprimir)

/**
 * Limites padrão de jerk (mm/s)
 * Sobreponha com M205 X Y Z . . . E
 *
 * "Jerk" define a menor variação de velocidade que exige aceleração.
 * Ao mudar de velocidade e direção, se a diferença for menor que o
 * valor definido aqui, a mudança pode acontecer instantaneamente.
 */
//#define CLASSIC_JERK
#if ENABLED(CLASSIC_JERK)
  #define DEFAULT_XJERK 10.0
  #define DEFAULT_YJERK 10.0
  #define DEFAULT_ZJERK  0.3
  #define DEFAULT_EJERK  5.0
  //#define DEFAULT_IJERK  0.3
  //#define DEFAULT_JJERK  0.3
  //#define DEFAULT_KJERK  0.3
  //#define DEFAULT_UJERK  0.3
  //#define DEFAULT_VJERK  0.3
  //#define DEFAULT_WJERK  0.3

  //#define TRAVEL_EXTRA_XYJERK 0.0     // Folga extra de jerk para todos os movimentos de deslocamento

  //#define LIMITED_JERK_EDITING        // Limita a edição via M205 ou LCD a DEFAULT_aJERK * 2
  #if ENABLED(LIMITED_JERK_EDITING)
    #define MAX_JERK_EDIT_VALUES { 20, 20, 0.6, 10 } // ...ou defina os seus próprios limites de edição
  #endif
#endif

/**
 * Fator de desvio de junção (Junction Deviation)
 *
 * Veja:
 *   https://reprap.org/forum/read.php?1,739819
 *   https://blog.kyneticcnc.com/2018/10/computing-junction-deviation-for-marlin.html
 */
#if DISABLED(CLASSIC_JERK)
  #define JUNCTION_DEVIATION_MM 0.013 // (mm) Distância da aresta real da junção
  #define JD_HANDLE_SMALL_SEGMENTS    // Usa estimativa de curvatura em vez de só o ângulo da junção
                                      // em segmentos pequenos (< 1mm) com ângulos de junção grandes (> 135°).
#endif

/**
 * Aceleração em curva S (S-Curve)
 *
 * Esta opção elimina vibração durante a impressão ajustando uma curva de
 * Bézier à aceleração do movimento, produzindo mudanças de direção bem mais suaves.
 *
 * Veja https://github.com/synthetos/TinyG/wiki/Jerk-Controlled-Motion-Explained
 */
//#define S_CURVE_ACCELERATION
#if ENABLED(S_CURVE_ACCELERATION)
  // Defina para usar curva de movimento de 4ª ordem em vez de 6ª
  //#define S_CURVE_FACTOR 0.25    // Fator de aceleração inicial e final, idealmente de 0.1 a 0.4.
                                   // Em geral não precisa de ajuste.
#endif

//===========================================================================
//======================== Opções do probe de Z =============================
//===========================================================================
// @section probes

//
// Veja https://marlinfw.org/docs/configuration/probes.html
//

/**
 * Habilite esta opção para um probe ligado ao pino Z-MIN.
 * O probe substitui o fim de curso Z-MIN e é usado para o homing de Z.
 * (Habilita USE_PROBE_FOR_Z_HOMING automaticamente.)
 */
//#define Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN   // Desabilitado: Z-MIN é um fim de curso real; o probe tem pino próprio

// Força o uso do probe para o homing do eixo Z
#define USE_PROBE_FOR_Z_HOMING   // G28 Z usa o probe do servo, não o sensor de camada 0

/**
 * Z_MIN_PROBE_PIN
 *
 * Só sobreponha este pino se o probe não puder ser ligado ao
 * Z_MIN_PROBE_PIN padrão da placa (MOTHERBOARD) selecionada.
 *
 *  - A opção mais simples é usar um conector de fim de curso livre.
 *  - Use 5V para sensores alimentados (normalmente os indutivos).
 *
 *  - Para chaves simples...
 *    - Normalmente fechada (NF) liga também ao GND.
 *    - Normalmente aberta (NA) liga também ao 5V.
 */
#define Z_MIN_PROBE_PIN 18   // EXP2_02 - entrada do probe do servo (ver nota de fiação)

/**
 * Usa o adaptador do pino 27 (na porta EXP) para o probe (e o beeper).
 * Habilite também USE_PROBE_FOR_Z_HOMING para BLTouch/3DTouch antigos
 * ligados ao Z_MIN_PIN.
 */
//#define USE_PIN_27_BOARD

/**
 * Tipo de probe
 *
 * Probes de chave Allen, probes com servo, probes em trenó, FIX_MOUNTED_PROBE, etc.
 * Ative um destes para usar o nivelamento automático de mesa mais abaixo.
 */

/**
 * O "probe manual" permite fazer o nivelamento "automático" de mesa sem um probe.
 * Use o G29 repetidamente, ajustando a altura Z em cada ponto com comandos de
 * movimento ou (com LCD_BED_LEVELING) pelo controlador do LCD.
 */
//#define PROBE_MANUALLY

/**
 * Um probe de montagem fixa não se desdobra ou precisa ser acionado manualmente.
 *   (ex.: um probe indutivo ou uma chave acionada pelo próprio bico.)
 */
//#define FIX_MOUNTED_PROBE

/**
 * Usa o bico como probe, como em sistemas de bico condutivo
 * ou em efetores inteligentes piezoelétricos.
 */
//#define NOZZLE_AS_PROBE

/**
 * Probe de Z acionado por servo, como uma chave de fim de curso num braço giratório.
 */
#define Z_PROBE_SERVO_NR 0
#ifdef Z_PROBE_SERVO_NR
  #define Z_SERVO_ANGLES { 35, 0 }        // Medidos na máquina: 35 = braço ativo (baixo), 0 = recolhido (livre)
  //#define Z_SERVO_MEASURE_ANGLE 45      // Use se o servo precisar ir para uma posição "livre" para medir após descer
  #define Z_SERVO_INTERMEDIATE_STOW       // Recolhe o probe entre os pontos
  //#define Z_SERVO_DEACTIVATE_AFTER_STOW // Desativa o servo quando o probe está recolhido
#endif

/**
 * O probe BLTouch usa um sensor de efeito Hall e emula um servo.
 */
//#define BLTOUCH

/**
 * Probe MagLev V4, da MDD
 *
 * Este probe é acionado e desce energizando um eletroímã interno.
 */
//#define MAGLEV4
#if ENABLED(MAGLEV4)
  //#define MAGLEV_TRIGGER_PIN 11     // Defina com a saída digital conectada
  #define MAGLEV_TRIGGER_DELAY 15     // Mudar isto arrisca superaquecer a bobina
#endif

/**
 * Probe Touch-MI, da hotends.fr
 *
 * Este probe desce e é ativado movendo o eixo X até um ímã na borda da mesa.
 * Por padrão, assume-se que o ímã está à esquerda e é acionado por um homing. Se o ímã
 * estiver à direita, habilite e ajuste TOUCH_MI_DEPLOY_XPOS com a posição de acionamento.
 *
 * Também requer: BABYSTEPPING, BABYSTEP_ZPROBE_OFFSET, Z_SAFE_HOMING,
 *                e um Z_CLEARANCE_FOR_HOMING mínimo de 10.
 */
//#define TOUCH_MI_PROBE
#if ENABLED(TOUCH_MI_PROBE)
  #define TOUCH_MI_RETRACT_Z 0.5                  // Altura em que o probe se recolhe
  //#define TOUCH_MI_DEPLOY_XPOS (X_MAX_BED + 2)  // Para um ímã no lado direito da mesa
  //#define TOUCH_MI_MANUAL_DEPLOY                // Para acionamento manual (menu do LCD)
#endif

/**
 * Sensor de distância da mesa (Bed Distance Sensor)
 *
 * Mede a distância da mesa ao bico com precisão de 0,01mm.
 * Informações sobre este sensor em https://github.com/markniu/Bed_Distance_sensor
 * Usa a porta I2C, então requer a biblioteca I2C markyue/Panda_SoftMasterI2C.
 */
//#define BD_SENSOR
#if ENABLED(BD_SENSOR)
  //#define BD_SENSOR_PROBE_NO_STOP // Sonda a mesa sem parar em cada ponto de sondagem
#endif

/**
 * BIQU MicroProbe
 *
 * Um probe leve, acionado por solenoide.
 * Informações sobre este sensor em https://github.com/bigtreetech/MicroProbe
 *
 * Também requer PROBE_ENABLE_DISABLE
 */
//#define BIQU_MICROPROBE_V1  // Dispara em HIGH
//#define BIQU_MICROPROBE_V2  // Dispara em LOW

// Um probe acionado e recolhido por um pino de solenoide (SOL1_PIN)
//#define SOLENOID_PROBE

// Um probe montado em trenó, como os projetados por Charles Bell.
//#define Z_PROBE_SLED
//#define SLED_DOCKING_OFFSET 5  // Distância extra que o eixo X precisa percorrer para pegar o trenó. 0 deve bastar, mas você pode aumentar se quiser.

// Um probe acionado pelo movimento do eixo X, como o de pinhão e cremalheira da Wilson II, projetado por Marty Rice.
//#define RACK_AND_PINION_PROBE
#if ENABLED(RACK_AND_PINION_PROBE)
  #define Z_PROBE_DEPLOY_X  X_MIN_POS
  #define Z_PROBE_RETRACT_X X_MAX_POS
#endif

/**
 * Probe de montagem magnética
 * Para probes como Euclid, Klicky, Klackender, etc.
 */
//#define MAG_MOUNTED_PROBE
#if ENABLED(MAG_MOUNTED_PROBE)
  #define PROBE_DEPLOY_FEEDRATE (133*60)  // (mm/min) Velocidade para acoplar o probe
  #define PROBE_STOW_FEEDRATE   (133*60)  // (mm/min) Velocidade para guardar o probe

  /**
   * Probe de montagem magnética com mecanismo de servo
   * Acoplar e guardar o probe seguem a mesma sequência básica:
   *  - Gira o SERVO para o ângulo de acoplamento
   *  - Executa movimentos XYZ para acoplar ou guardar o PROBE
   *  - Gira o SERVO para o ângulo de repouso
   */
  //#define MAG_MOUNTED_PROBE_SERVO_NR 0             // Número do servo deste probe
  #ifdef MAG_MOUNTED_PROBE_SERVO_NR
    #define MAG_MOUNTED_PROBE_SERVO_ANGLES { 90, 0 } // Ângulos do servo para acoplado e guardado
    #define MAG_MOUNTED_PRE_DEPLOY { PROBE_DEPLOY_FEEDRATE, { 15, 160, 30 } }  // Posição segura para acionar o servo
    #define MAG_MOUNTED_PRE_STOW   { PROBE_DEPLOY_FEEDRATE, { 15, 160, 30 } }  // Posição segura para desacionar o servo
  #endif

  #define MAG_MOUNTED_DEPLOY_1 { PROBE_DEPLOY_FEEDRATE, { 245, 114, 30 } }  // Vai até a doca lateral e acopla o probe
  #define MAG_MOUNTED_DEPLOY_2 { PROBE_DEPLOY_FEEDRATE, { 210, 114, 30 } }  // Tira o probe da doca
  #define MAG_MOUNTED_DEPLOY_3 { PROBE_DEPLOY_FEEDRATE, {   0,   0,  0 } }  // Movimento extra, se necessário
  #define MAG_MOUNTED_DEPLOY_4 { PROBE_DEPLOY_FEEDRATE, {   0,   0,  0 } }  // Movimento extra, se necessário
  #define MAG_MOUNTED_DEPLOY_5 { PROBE_DEPLOY_FEEDRATE, {   0,   0,  0 } }  // Movimento extra, se necessário
  #define MAG_MOUNTED_STOW_1   { PROBE_STOW_FEEDRATE,   { 245, 114, 20 } }  // Vai até a doca
  #define MAG_MOUNTED_STOW_2   { PROBE_STOW_FEEDRATE,   { 245, 114,  0 } }  // Posiciona o probe ao lado do removedor
  #define MAG_MOUNTED_STOW_3   { PROBE_STOW_FEEDRATE,   { 230, 114,  0 } }  // Movimento lateral para soltar o probe
  #define MAG_MOUNTED_STOW_4   { PROBE_STOW_FEEDRATE,   { 210, 114, 20 } }  // Movimento lateral para soltar o probe
  #define MAG_MOUNTED_STOW_5   { PROBE_STOW_FEEDRATE,   {   0,   0,  0 } }  // Movimento extra, se necessário
#endif

// Duet Smart Effector (para impressoras delta) - https://docs.duet3d.com/en/Duet3D_hardware/Accessories/Smart_Effector
// Com o pino definido, você pode usar o M672 para ajustar/zerar a sensibilidade do probe.
//#define DUET_SMART_EFFECTOR
#if ENABLED(DUET_SMART_EFFECTOR)
  #define SMART_EFFECTOR_MOD_PIN  -1  // Ligue um pino GPIO ao pino MOD do Smart Effector
#endif

/**
 * Usa o StallGuard2 para sondar a mesa com o próprio bico.
 * Requer drivers Trinamic com suporte a stallGuard.
 * CUIDADO: isso pode danificar máquinas com fusos no eixo Z.
 *          Tenha extrema cautela ao configurar este recurso.
 */
//#define SENSORLESS_PROBING

/**
 * Probe de Z retrátil por chave Allen, visto em muitas impressoras delta Kossel - https://reprap.org/wiki/Kossel#Autolevel_probe
 * Desce encostando na correia do eixo Z. Recolhe empurrando o probe para baixo.
 */
//#define Z_PROBE_ALLEN_KEY
#if ENABLED(Z_PROBE_ALLEN_KEY)
  // 2 ou 3 conjuntos de coordenadas para acionar e recolher o probe com mola no G29,
  // caso o probe acionado por servo não esteja definido. Descomente conforme a sua impressora/probe.

  #define Z_PROBE_ALLEN_KEY_DEPLOY_1 { 30.0, PRINTABLE_RADIUS, 100.0 }
  #define Z_PROBE_ALLEN_KEY_DEPLOY_1_FEEDRATE XY_PROBE_FEEDRATE

  #define Z_PROBE_ALLEN_KEY_DEPLOY_2 { 0.0, PRINTABLE_RADIUS, 100.0 }
  #define Z_PROBE_ALLEN_KEY_DEPLOY_2_FEEDRATE (XY_PROBE_FEEDRATE)/10

  #define Z_PROBE_ALLEN_KEY_DEPLOY_3 { 0.0, (PRINTABLE_RADIUS) * 0.75, 100.0 }
  #define Z_PROBE_ALLEN_KEY_DEPLOY_3_FEEDRATE XY_PROBE_FEEDRATE

  #define Z_PROBE_ALLEN_KEY_STOW_1 { -64.0, 56.0, 23.0 } // Leva o probe até a posição
  #define Z_PROBE_ALLEN_KEY_STOW_1_FEEDRATE XY_PROBE_FEEDRATE

  #define Z_PROBE_ALLEN_KEY_STOW_2 { -64.0, 56.0, 3.0 } // Empurra para baixo
  #define Z_PROBE_ALLEN_KEY_STOW_2_FEEDRATE (XY_PROBE_FEEDRATE)/10

  #define Z_PROBE_ALLEN_KEY_STOW_3 { -64.0, 56.0, 50.0 } // Sobe para liberar
  #define Z_PROBE_ALLEN_KEY_STOW_3_FEEDRATE XY_PROBE_FEEDRATE

  #define Z_PROBE_ALLEN_KEY_STOW_4 { 0.0, 0.0, 50.0 }
  #define Z_PROBE_ALLEN_KEY_STOW_4_FEEDRATE XY_PROBE_FEEDRATE

#endif // Z_PROBE_ALLEN_KEY

/**
 * Offsets do bico até o probe { X, Y, Z }
 *
 * Offset de X e Y
 *   Use um paquímetro ou régua para medir a distância (em mm) da ponta do
 *   bico até o ponto central do probe, nos eixos X e Y.
 *
 * Offset de Z
 * - Para o offset de Z, use o melhor valor que você conhece e ajuste em tempo de execução.
 * - Os probes comuns disparam abaixo do bico e têm valores negativos de offset Z.
 * - Probes que disparam acima da altura do bico são incomuns, mas existem. Ao usar um
 *   probe assim, ajuste com cuidado Z_CLEARANCE_DEPLOY_PROBE e Z_CLEARANCE_BETWEEN_PROBES
 *   para evitar colisões durante a sondagem.
 *
 * Ajuste fino
 * -  Os offsets do probe podem ser ajustados em tempo de execução com 'M851', menus do LCD, babystepping, etc.
 * -  O PROBE_OFFSET_WIZARD (Configuration_adv.h) pode ser usado para definir o offset de Z.
 *
 * Considerando a orientação típica da área de trabalho:
 *  - Probe à DIREITA do bico tem offset X positivo
 *  - Probe à ESQUERDA do bico tem offset X negativo
 *  - Probe ATRÁS do bico tem offset Y positivo
 *  - Probe À FRENTE do bico tem offset Y negativo
 *
 * Alguns exemplos:
 *   #define NOZZLE_TO_PROBE_OFFSET { 10, 10, -1 }   // Exemplo "1"
 *   #define NOZZLE_TO_PROBE_OFFSET {-10,  5, -1 }   // Exemplo "2"
 *   #define NOZZLE_TO_PROBE_OFFSET {  5, -5, -1 }   // Exemplo "3"
 *   #define NOZZLE_TO_PROBE_OFFSET {-15,-10, -1 }   // Exemplo "4"
 *
 *     +-- FUNDO --+
 *     |    [+]    |
 *   E |        1  | D <-- Exemplo "1" (direita+,  fundo+)
 *   S |  2        | I <-- Exemplo "2" (esquerda-, fundo+)
 *   Q |[-]  N  [+]| R <-- Bico (Nozzle)
 *   . |       3   | . <-- Exemplo "3" (direita+,  frente-)
 *     | 4         |   <-- Exemplo "4" (esquerda-, frente-)
 *     |    [-]    |
 *     O-- FRENTE -+
 */
// Medido com paquímetro:
//   X = -38 : probe 38mm à ESQUERDA do bico
//   Y = +10 : probe 10mm ATRÁS do bico
//   Z =   0 : PROVISÓRIO, ainda falta medir (probe abaixo do bico = negativo)
#define NOZZLE_TO_PROBE_OFFSET { -38, 10, 0 } // (mm) X, Y, Z da ponta do bico ao ponto de disparo

// Habilite e defina para usar uma ferramenta específica na sondagem. Desabilite para permitir qualquer ferramenta.
#define PROBING_TOOL 0
#ifdef PROBING_TOOL
  //#define PROBE_TOOLCHANGE_NO_MOVE  // Suprime o movimento na troca de ferramenta para sondagem
#endif

//#define PROBE_WAKEUP_TIME_MS  30    // (ms) Tempo para o probe "acordar"

// A maioria dos probes deve ficar longe das bordas da mesa, mas
// com NOZZLE_AS_PROBE este valor pode ser negativo, para uma área de sondagem maior.
// Parafusos de 10x10mm nos 4 cantos: a margem mantém os pontos da malha fora deles.
#define PROBING_MARGIN 15

// Velocidade de deslocamento dos eixos X e Y entre as sondagens.
// Deixe indefinido para usar a média da velocidade de homing XY atual.
#define XY_PROBE_FEEDRATE     (60*60) // (mm/min) 60mm/s. O padrão de 133mm/s roncava (perda de passos em 12V).

// Velocidade da primeira aproximação na sondagem dupla (MULTIPLE_PROBING == 2)
#define Z_PROBE_FEEDRATE_FAST  (4*60) // (mm/min)

// Velocidade da sondagem "precisa" de cada ponto
#define Z_PROBE_FEEDRATE_SLOW (Z_PROBE_FEEDRATE_FAST / 2) // (mm/min)

/**
 * Chave de ativação do probe
 * Uma chave que indica o acionamento correto, ou uma chave
 * óptica disparada quando o carro se aproxima da mesa.
 */
//#define PROBE_ACTIVATION_SWITCH
#if ENABLED(PROBE_ACTIVATION_SWITCH)
  #define PROBE_ACTIVATION_SWITCH_STATE LOW // Estado que indica probe ativo
  //#define PROBE_ACTIVATION_SWITCH_PIN PC6 // Sobrepõe o pino padrão
#endif

/**
 * Tara do probe (determina o ponto zero) antes de cada sondagem.
 * Útil para extensômetros (strain gauge) ou sensores piezoelétricos, que precisam
 * descontar fatores como cabos puxando o carro.
 */
//#define PROBE_TARE
#if ENABLED(PROBE_TARE)
  #define PROBE_TARE_TIME  200    // (ms) Tempo mantendo o pino de tara acionado
  #define PROBE_TARE_DELAY 200    // (ms) Atraso após a tara
  #define PROBE_TARE_STATE HIGH   // Estado a escrever no pino para tarar
  //#define PROBE_TARE_PIN PA5    // Sobrepõe o pino padrão
  //#define PROBE_TARE_MENU       // Mostra um item de menu para tarar o probe
  #if ENABLED(PROBE_ACTIVATION_SWITCH)
    //#define PROBE_TARE_ONLY_WHILE_INACTIVE  // Falha ao tarar/sondar se PROBE_ACTIVATION_SWITCH estiver ativo
  #endif
#endif

/**
 * Habilitar / desabilitar o probe
 * O probe só fornece sinal de disparo quando está habilitado.
 */
//#define PROBE_ENABLE_DISABLE
#if ENABLED(PROBE_ENABLE_DISABLE)
  //#define PROBE_ENABLE_PIN -1   // Sobrepõe o pino padrão aqui
#endif

/**
 * Sondagem múltipla
 *
 * Você pode obter resultados melhores sondando 2 ou mais vezes.
 * Com EXTRA_PROBING, as leituras mais atípicas são descartadas.
 *
 * Um total de 2 faz sondagens rápida/lenta com média ponderada.
 * Um total de 3 ou mais acrescenta sondagens lentas e tira a média.
 */
//#define MULTIPLE_PROBING 2
//#define EXTRA_PROBING    1

/**
 * Os probes de Z precisam de folga ao descer, recolher e se deslocar entre
 * pontos de sondagem, para não bater na mesa nem em outras peças.
 * Probes montados em servo precisam de espaço extra para o braço girar.
 * Probes indutivos precisam de espaço para não dispararem antes da hora.
 *
 * Use estes ajustes para especificar a distância (mm) de elevação do probe (ou
 * de descida da mesa). Os valores aqui se somam a qualquer offset Z (negativo)
 * do probe definido por NOZZLE_TO_PROBE_OFFSET, M851 ou pelo LCD.
 * Só valores inteiros >= 1 são válidos aqui.
 *
 * Exemplo: 'M851 Z-5' com uma folga (CLEARANCE) de 4  =>  9mm da mesa ao bico.
 *     Mas: 'M851 Z+1' com uma folga (CLEARANCE) de 2  =>  2mm da mesa ao bico.
 */
#define Z_CLEARANCE_DEPLOY_PROBE   10 // (mm) Folga de Z para acionar/recolher o probe
#define Z_CLEARANCE_BETWEEN_PROBES  8 // (mm) 8 > 4mm dos parafusos de fixação da mesa
#define Z_CLEARANCE_MULTI_PROBE     8 // (mm) 8 > 4mm dos parafusos de fixação da mesa
#define Z_PROBE_ERROR_TOLERANCE     3 // (mm) Tolerância para disparo antecipado (<= -probe.offset.z + ZPET)
//#define Z_AFTER_PROBING           5 // (mm) Posição Z depois de concluída a sondagem

#define Z_PROBE_LOW_POINT          -2 // (mm) Maior distância abaixo do ponto de disparo até parar

// Para o M851, define as faixas de ajuste dos offsets X, Y e Z do probe
//#define PROBE_OFFSET_XMIN -50   // (mm)
//#define PROBE_OFFSET_XMAX  50   // (mm)
//#define PROBE_OFFSET_YMIN -50   // (mm)
//#define PROBE_OFFSET_YMAX  50   // (mm)
//#define PROBE_OFFSET_ZMIN -20   // (mm)
//#define PROBE_OFFSET_ZMAX  20   // (mm)

// Habilita o teste de repetibilidade M48, para avaliar a precisão do probe
//#define Z_MIN_PROBE_REPEATABILITY_TEST

// Pausa para confirmação do usuário antes de acionar/recolher o probe
//#define PAUSE_BEFORE_DEPLOY_STOW
#if ENABLED(PAUSE_BEFORE_DEPLOY_STOW)
  //#define PAUSE_PROBE_DEPLOY_WHEN_TRIGGERED // Para probe de chave Allen com acionamento manual
#endif

/**
 * Habilite um ou mais dos itens a seguir se a sondagem parecer pouco confiável.
 * Aquecedores e/ou ventoinhas podem ser desligados durante a sondagem para reduzir
 * ruído elétrico. Também é possível adicionar um atraso para o ruído e a vibração
 * assentarem. Estas opções são mais úteis com o probe BLTouch, mas também podem
 * melhorar as leituras de probes indutivos e sensores piezoelétricos.
 */
//#define PROBING_HEATERS_OFF       // Desliga os aquecedores durante a sondagem
#if ENABLED(PROBING_HEATERS_OFF)
  //#define WAIT_FOR_BED_HEATER     // Espera a mesa reaquecer entre as sondagens (melhora a precisão)
  //#define WAIT_FOR_HOTEND         // Espera o hotend reaquecer entre as sondagens (melhora a precisão e evita extrusão a frio)
#endif
//#define PROBING_FANS_OFF          // Desliga as ventoinhas durante a sondagem
//#define PROBING_ESTEPPERS_OFF     // Desliga todos os motores de extrusora durante a sondagem
//#define PROBING_STEPPERS_OFF      // Desliga todos os motores (salvo os necessários para manter a posição) durante a sondagem, inclusive extrusoras
//#define DELAY_BEFORE_PROBING 200  // (ms) Para evitar que vibrações disparem sensores piezoelétricos

// Exige temperatura mínima do bico e/ou da mesa para sondar
//#define PREHEAT_BEFORE_PROBING
#if ENABLED(PREHEAT_BEFORE_PROBING)
  #define PROBING_NOZZLE_TEMP 120   // (°C) Por enquanto vale só para o E0
  #define PROBING_BED_TEMP     50
#endif

// @section stepper drivers

// Para pinos de habilitação invertidos (ativo em nível baixo) use 0; não invertidos (ativo em nível alto) use 1
// :['LOW', 'HIGH']
#define X_ENABLE_ON LOW
#define Y_ENABLE_ON LOW
#define Z_ENABLE_ON LOW
#define E_ENABLE_ON LOW // Para todas as extrusoras
//#define I_ENABLE_ON LOW
//#define J_ENABLE_ON LOW
//#define K_ENABLE_ON LOW
//#define U_ENABLE_ON LOW
//#define V_ENABLE_ON LOW
//#define W_ENABLE_ON LOW

// Desliga os motores do eixo assim que eles param de receber passos.
// ATENÇÃO: quando os motores desligam, há chance de perder a precisão de posição!
//#define DISABLE_X
//#define DISABLE_Y
//#define DISABLE_Z
//#define DISABLE_I
//#define DISABLE_J
//#define DISABLE_K
//#define DISABLE_U
//#define DISABLE_V
//#define DISABLE_W

// @section extruder

//#define DISABLE_E               // Desliga a extrusora quando ela não está recebendo passos
#define DISABLE_OTHER_EXTRUDERS   // Mantém ligada apenas a extrusora ativa

// @section stepper drivers

// Inverte o sentido do motor. Altere (ou inverta o conector do motor) se um eixo andar para o lado errado.
#define INVERT_X_DIR false
#define INVERT_Y_DIR false  // CoreXY: motor B (conector Y). Corrigido após teste de direção.
#define INVERT_Z_DIR false
//#define INVERT_I_DIR false
//#define INVERT_J_DIR false
//#define INVERT_K_DIR false
//#define INVERT_U_DIR false
//#define INVERT_V_DIR false
//#define INVERT_W_DIR false

// Para extrusora direct drive v9 use true; para extrusora com redução (engrenada) use false.
#define INVERT_E0_DIR false
#define INVERT_E1_DIR false
#define INVERT_E2_DIR false
#define INVERT_E3_DIR false
#define INVERT_E4_DIR false
#define INVERT_E5_DIR false
#define INVERT_E6_DIR false
#define INVERT_E7_DIR false

// @section homing

#define NO_MOTION_BEFORE_HOMING   // Sem G28 o Marlin não sabe onde está: os limites de software não valem.
#define HOME_AFTER_DEACTIVATE     // Após M18/timeout o carro pode ter sido movido à mão: exige novo G28.

/**
 * Defina Z_IDLE_HEIGHT se o eixo Z se mover sozinho quando os motores forem desligados.
 *  - Use um valor baixo (ex.: Z_MIN_POS) se o bico cair em direção à mesa.
 *  - Use um valor alto (ex.: Z_MAX_POS) se a mesa cair, afastando-se do bico.
 */
//#define Z_IDLE_HEIGHT Z_HOME_POS

#define Z_CLEARANCE_FOR_HOMING  8   // (mm) Sobe antes do G28 de X/Y. 8 > 4mm dos parafusos da mesa.

#define Z_AFTER_HOMING         10   // (mm) Termina todo G28 numa altura segura
//#define XY_AFTER_HOMING { 10, 10 }  // (mm) Posição XY para onde ir depois do homing (e de levantar o Z)

//#define EVENT_GCODE_AFTER_HOMING "M300 P440 S200"  // Comandos a executar depois do G28 (e do movimento até XY_AFTER_HOMING)

// Sentido dos fins de curso no homing; 1=MAX, -1=MIN
// :[-1,1]
#define X_HOME_DIR  1   // X faz home no MÁXIMO (direita). O probe bate no lado mínimo.
#define Y_HOME_DIR -1
#define Z_HOME_DIR -1   // Z faz home no MÍNIMO (sensor de camada 0)
//#define I_HOME_DIR -1
//#define J_HOME_DIR -1
//#define K_HOME_DIR -1
//#define U_HOME_DIR -1
//#define V_HOME_DIR -1
//#define W_HOME_DIR -1

/**
 * Paradas de segurança
 * Se um eixo tem fins de curso nas duas pontas, o indicado acima é usado para
 * o homing, enquanto o outro pode servir para coisas como SD_ABORT_ON_ENDSTOP_HIT.
 */
//#define X_SAFETY_STOP
//#define Y_SAFETY_STOP
//#define Z_SAFETY_STOP
//#define I_SAFETY_STOP
//#define J_SAFETY_STOP
//#define K_SAFETY_STOP
//#define U_SAFETY_STOP
//#define V_SAFETY_STOP
//#define W_SAFETY_STOP

// @section geometry

// O tamanho da área imprimível
// Medido na máquina, com o bico posicionado nas bordas:
//   X: borda esquerda e direita da área útil
//   Y: borda da frente e do fundo (o fundo é também o limite mecânico, 2mm de folga)
// As coordenadas foram deslocadas (+3 em X, -1 em Y) para as bordas caírem em zero,
// porque o Marlin sempre assume a mesa começando em 0 (X_MIN_BED = 0).
#define X_BED_SIZE 201
#define Y_BED_SIZE 193

// Limites de curso (linear=mm, rotacional=°) após o homing, correspondendo às
// posições dos fins de curso.
#define X_MIN_POS 0     // Borda esquerda da mesa. LIMITE ABSOLUTO: o probe bate no
                        // mecanismo antes do sensor X min, que por isso não protege
                        // esse lado. Há 5mm de folga física, deixados de reserva.
                        // REVISAR se a montagem do probe mudar.
#define Y_MIN_POS -6    // Posição do sensor Y min (home), 6mm à frente da mesa.
                        // Não é escolha: o home no mínimo fixa esta coordenada.
#define Z_MIN_POS 0
#define X_MAX_POS 208   // Posição do sensor X max (home), 7mm além da borda direita
#define Y_MAX_POS 193   // Borda do fundo = limite mecânico (só 2mm de folga real)
#define Z_MAX_POS 250
//#define I_MIN_POS 0
//#define I_MAX_POS 50
//#define J_MIN_POS 0
//#define J_MAX_POS 50
//#define K_MIN_POS 0
//#define K_MAX_POS 50
//#define U_MIN_POS 0
//#define U_MAX_POS 50
//#define V_MIN_POS 0
//#define V_MAX_POS 50
//#define W_MIN_POS 0
//#define W_MAX_POS 50

/**
 * Fins de curso por software
 *
 * - Impedem movimentos fora dos limites definidos da máquina.
 * - Eixos individuais podem ser desabilitados, se desejado.
 * - X e Y só se aplicam a robôs cartesianos.
 * - Use 'M211' para ligar/desligar os fins de curso por software ou consultar o estado
 */

// Os fins de curso mínimos por software restringem o movimento ao limite inferior
#define MIN_SOFTWARE_ENDSTOPS
#if ENABLED(MIN_SOFTWARE_ENDSTOPS)
  #define MIN_SOFTWARE_ENDSTOP_X
  #define MIN_SOFTWARE_ENDSTOP_Y
  #define MIN_SOFTWARE_ENDSTOP_Z
  #define MIN_SOFTWARE_ENDSTOP_I
  #define MIN_SOFTWARE_ENDSTOP_J
  #define MIN_SOFTWARE_ENDSTOP_K
  #define MIN_SOFTWARE_ENDSTOP_U
  #define MIN_SOFTWARE_ENDSTOP_V
  #define MIN_SOFTWARE_ENDSTOP_W
#endif

// Os fins de curso máximos por software restringem o movimento ao limite superior
#define MAX_SOFTWARE_ENDSTOPS
#if ENABLED(MAX_SOFTWARE_ENDSTOPS)
  #define MAX_SOFTWARE_ENDSTOP_X
  #define MAX_SOFTWARE_ENDSTOP_Y
  #define MAX_SOFTWARE_ENDSTOP_Z
  #define MAX_SOFTWARE_ENDSTOP_I
  #define MAX_SOFTWARE_ENDSTOP_J
  #define MAX_SOFTWARE_ENDSTOP_K
  #define MAX_SOFTWARE_ENDSTOP_U
  #define MAX_SOFTWARE_ENDSTOP_V
  #define MAX_SOFTWARE_ENDSTOP_W
#endif

#if ANY(MIN_SOFTWARE_ENDSTOPS, MAX_SOFTWARE_ENDSTOPS)
  //#define SOFT_ENDSTOPS_MENU_ITEM  // Liga/desliga os fins de curso por software pelo LCD
#endif

/**
 * @section filament runout sensors
 *
 * Sensores de fim de filamento
 * Fins de curso mecânicos ou ópticos são usados para verificar a presença do filamento.
 *
 * IMPORTANTE: o fim de filamento só dispara se o Marlin souber que há uma impressão
 * em andamento. O Marlin sabe disso quando:
 *  1. Uma impressão da mídia foi iniciada com M24.
 *  2. O temporizador de impressão foi iniciado com M75.
 *  3. Os aquecedores foram ligados e PRINTJOB_TIMER_AUTOSTART está habilitado.
 *
 * Placas baseadas em RAMPS usam o SERVO3_PIN para o primeiro sensor.
 * Em outras placas pode ser necessário definir FIL_RUNOUT_PIN, FIL_RUNOUT2_PIN, etc.
 */
#define FILAMENT_RUNOUT_SENSOR
#if ENABLED(FILAMENT_RUNOUT_SENSOR)
  #define FIL_RUNOUT_ENABLED_DEFAULT true // Habilita o sensor no boot. Sobreponha com M412 seguido de M500.
  #define NUM_RUNOUT_SENSORS   1          // Número de sensores, até um por extrusora. Defina um FIL_RUNOUT#_PIN para cada.

  #define FIL_RUNOUT_STATE     LOW        // Estado do pino que indica que o filamento NÃO está presente.
  #define FIL_RUNOUT_PULLUP               // Usa o pull-up interno nos pinos de fim de filamento.
  //#define FIL_RUNOUT_PULLDOWN           // Usa o pull-down interno nos pinos de fim de filamento.
  //#define WATCH_ALL_RUNOUT_SENSORS      // Executa o script em qualquer sensor que disparar, não só o da extrusora ativa.
                                          // Isso é habilitado automaticamente para MIXING_EXTRUDER.

  // Sobreponha individualmente se os sensores forem diferentes entre si
  //#define FIL_RUNOUT1_STATE LOW
  //#define FIL_RUNOUT1_PULLUP
  //#define FIL_RUNOUT1_PULLDOWN

  //#define FIL_RUNOUT2_STATE LOW
  //#define FIL_RUNOUT2_PULLUP
  //#define FIL_RUNOUT2_PULLDOWN

  //#define FIL_RUNOUT3_STATE LOW
  //#define FIL_RUNOUT3_PULLUP
  //#define FIL_RUNOUT3_PULLDOWN

  //#define FIL_RUNOUT4_STATE LOW
  //#define FIL_RUNOUT4_PULLUP
  //#define FIL_RUNOUT4_PULLDOWN

  //#define FIL_RUNOUT5_STATE LOW
  //#define FIL_RUNOUT5_PULLUP
  //#define FIL_RUNOUT5_PULLDOWN

  //#define FIL_RUNOUT6_STATE LOW
  //#define FIL_RUNOUT6_PULLUP
  //#define FIL_RUNOUT6_PULLDOWN

  //#define FIL_RUNOUT7_STATE LOW
  //#define FIL_RUNOUT7_PULLUP
  //#define FIL_RUNOUT7_PULLDOWN

  //#define FIL_RUNOUT8_STATE LOW
  //#define FIL_RUNOUT8_PULLUP
  //#define FIL_RUNOUT8_PULLDOWN

  // Comandos a executar quando acabar o filamento.
  // Com vários sensores, use o marcador %c para a ferramenta atual (ex.: "M600 T%c")
  // OBS.: depois de 'M412 H1' quem trata o fim de filamento é o host, e este script não se aplica.
  #define FILAMENT_RUNOUT_SCRIPT "M25"   // M600 exige ADVANCED_PAUSE_FEATURE (indisponível sem LCD com menu)

  // Após detectar o fim do filamento, continua imprimindo este comprimento antes
  // de executar o script. Útil para sensor instalado no fim de um tubo de
  // alimentação. Custa 4 bytes de SRAM por sensor, mais 4 bytes de overhead.
  //#define FILAMENT_RUNOUT_DISTANCE_MM 25

  #ifdef FILAMENT_RUNOUT_DISTANCE_MM
    // Habilite esta opção para usar um disco encoder que alterna o pino conforme
    // o filamento se move. (Garanta um FILAMENT_MOTION_DISTANCE_MM grande o
    // bastante para evitar falsos positivos.)
    //#define FILAMENT_MOTION_SENSOR

    #if ENABLED(FILAMENT_MOTION_SENSOR)
      //#define FILAMENT_SWITCH_AND_MOTION      // Define pinos separados abaixo para detectar o movimento
      #if ENABLED(FILAMENT_SWITCH_AND_MOTION)

        #define FILAMENT_MOTION_DISTANCE_MM 3.0 // (mm) Distância faltante para disparar (0 = sem detecção de entupimento)

        #define NUM_MOTION_SENSORS   1          // Número de sensores, até um por extrusora. Defina um FIL_MOTION#_PIN para cada.
        //#define FIL_MOTION1_PIN    -1

        // Sobreponha individualmente se os sensores de movimento forem diferentes
        //#define FIL_MOTION1_STATE LOW
        //#define FIL_MOTION1_PULLUP
        //#define FIL_MOTION1_PULLDOWN

        //#define FIL_MOTION2_STATE LOW
        //#define FIL_MOTION2_PULLUP
        //#define FIL_MOTION2_PULLDOWN

        //#define FIL_MOTION3_STATE LOW
        //#define FIL_MOTION3_PULLUP
        //#define FIL_MOTION3_PULLDOWN

        //#define FIL_MOTION4_STATE LOW
        //#define FIL_MOTION4_PULLUP
        //#define FIL_MOTION4_PULLDOWN

        //#define FIL_MOTION5_STATE LOW
        //#define FIL_MOTION5_PULLUP
        //#define FIL_MOTION5_PULLDOWN

        //#define FIL_MOTION6_STATE LOW
        //#define FIL_MOTION6_PULLUP
        //#define FIL_MOTION6_PULLDOWN

        //#define FIL_MOTION7_STATE LOW
        //#define FIL_MOTION7_PULLUP
        //#define FIL_MOTION7_PULLDOWN

        //#define FIL_MOTION8_STATE LOW
        //#define FIL_MOTION8_PULLUP
        //#define FIL_MOTION8_PULLDOWN
      #endif // FILAMENT_SWITCH_AND_MOTION
    #endif // FILAMENT_MOTION_SENSOR
  #endif // FILAMENT_RUNOUT_DISTANCE_MM
#endif // FILAMENT_RUNOUT_SENSOR

//===========================================================================
//========================= Nivelamento de mesa =============================
//===========================================================================
// @section calibrate

/**
 * Escolha uma das opções abaixo para habilitar o nivelamento de mesa por G29.
 * Os parâmetros e o comportamento do G29 mudam conforme a sua escolha.
 *
 *  Se usar um probe para referenciar o Z, habilite também o Z_SAFE_HOMING!
 *
 * - AUTO_BED_LEVELING_3POINT
 *   Sonda 3 pontos arbitrários da mesa (que não sejam colineares).
 *   Você especifica as coordenadas XY dos 3 pontos.
 *   O resultado é um único plano inclinado. Melhor para mesa plana.
 *
 * - AUTO_BED_LEVELING_LINEAR
 *   Sonda vários pontos numa grade.
 *   Você especifica o retângulo e a densidade dos pontos.
 *   O resultado é um único plano inclinado. Melhor para mesa plana.
 *
 * - AUTO_BED_LEVELING_BILINEAR
 *   Sonda vários pontos numa grade.
 *   Você especifica o retângulo e a densidade dos pontos.
 *   O resultado é uma malha, melhor para mesas grandes ou irregulares.
 *
 * - AUTO_BED_LEVELING_UBL (Unified Bed Leveling)
 *   Um sistema abrangente de nivelamento, que combina recursos e vantagens dos
 *   demais. O UBL também inclui geração, validação e edição de malha integradas.
 *
 * - MESH_BED_LEVELING
 *   Sonda uma grade manualmente.
 *   O resultado é uma malha, adequada a mesas grandes ou irregulares. (Veja BILINEAR.)
 *   Para máquinas sem probe, oferece um método de nivelar por etapas, ajustando
 *   a altura Z manualmente em cada ponto da grade.
 *   Com um controlador de LCD o processo é guiado passo a passo.
 */
//#define AUTO_BED_LEVELING_3POINT
//#define AUTO_BED_LEVELING_LINEAR
#define AUTO_BED_LEVELING_BILINEAR
//#define AUTO_BED_LEVELING_UBL
//#define MESH_BED_LEVELING

/**
 * Comandos a executar no início da sondagem do G29,
 * depois de trocar para a PROBING_TOOL.
 */
//#define EVENT_GCODE_BEFORE_G29 "M300 P440 S200"

/**
 * Comandos a executar no fim da sondagem do G29.
 * Útil para retrair ou tirar o probe de Z do caminho.
 */
//#define EVENT_GCODE_AFTER_G29 "G1 Z10 F12000\nG1 X15 Y330\nG1 Z0.5\nG1 Z10"

/**
 * Normalmente o G28 termina com o nivelamento desabilitado. Habilite uma destas
 * opções para restaurar o estado anterior do nivelamento ou para sempre
 * habilitá-lo logo após o G28.
 */
//#define RESTORE_LEVELING_AFTER_G28
//#define ENABLE_LEVELING_AFTER_G28

/**
 * O nivelamento automático precisa de pré-aquecimento
 */
//#define PREHEAT_BEFORE_LEVELING
#if ENABLED(PREHEAT_BEFORE_LEVELING)
  #define LEVELING_NOZZLE_TEMP 120   // (°C) Por enquanto vale só para o E0
  #define LEVELING_BED_TEMP     50
#endif

/**
 * Habilita o registro detalhado de G28, G29, M48, etc.
 * Ligue com o comando 'M111 S32'.
 * OBS.: consome bastante flash!
 */
//#define DEBUG_LEVELING_FEATURE

#if ANY(MESH_BED_LEVELING, AUTO_BED_LEVELING_UBL, PROBE_MANUALLY)
  // Define uma altura para o início do ajuste manual
  #define MANUAL_PROBE_START_Z 0.2  // (mm) Comente para usar a última altura medida
#endif

#if ANY(MESH_BED_LEVELING, AUTO_BED_LEVELING_BILINEAR, AUTO_BED_LEVELING_UBL)
  /**
   * Reduz gradualmente a correção do nivelamento até uma certa altura, a partir
   * da qual o movimento passa a ser paralelo ao plano XY da máquina.
   * A altura pode ser definida com M420 Z<altura>
   */
  #define ENABLE_LEVELING_FADE_HEIGHT
  #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
    #define DEFAULT_LEVELING_FADE_HEIGHT 10.0 // (mm) Altura padrão de desvanecimento.
  #endif

  /**
   * Em máquinas cartesianas, em vez de dividir os movimentos nas fronteiras da
   * malha, quebra-os em segmentos curtos como numa Delta. Isso acompanha o
   * contorno da mesa mais de perto que movimentos retos de ponta a ponta.
   */
  #define SEGMENT_LEVELED_MOVES
  #define LEVELED_SEGMENT_LENGTH 5.0 // (mm) Comprimento de todos os segmentos (exceto o último)

  /**
   * Habilita a ferramenta de padrão de validação de malha G26.
   */
  //#define G26_MESH_VALIDATION
  #if ENABLED(G26_MESH_VALIDATION)
    #define MESH_TEST_NOZZLE_SIZE    0.4  // (mm) Diâmetro do bico principal.
    #define MESH_TEST_LAYER_HEIGHT   0.2  // (mm) Altura de camada padrão do G26.
    #define MESH_TEST_HOTEND_TEMP  205    // (°C) Temperatura padrão do bico no G26.
    #define MESH_TEST_BED_TEMP      60    // (°C) Temperatura padrão da mesa no G26.
    #define G26_XY_FEEDRATE         20    // (mm/s) Velocidade dos movimentos XY do G26.
    #define G26_XY_FEEDRATE_TRAVEL 100    // (mm/s) Velocidade de deslocamento XY do G26.
    #define G26_RETRACT_MULTIPLIER   1.0  // Retração (G26 Q) usada por padrão entre elementos do teste.
  #endif

  #define MAX_SAVED_MESHES    100         // Máximo de malhas gravadas na EEPROM. Use 0 para desabilitar.

#endif

#if ANY(AUTO_BED_LEVELING_LINEAR, AUTO_BED_LEVELING_BILINEAR)

  // Define o número de pontos da grade por dimensão.
  #define GRID_MAX_POINTS_X 3
  #define GRID_MAX_POINTS_Y GRID_MAX_POINTS_X

  // Sonda ao longo do eixo Y, avançando o X a cada coluna
  //#define PROBE_Y_FIRST

  #if ENABLED(AUTO_BED_LEVELING_BILINEAR)

    #if DISABLED(USE_PROBE_FOR_Z_HOMING) && ALL(FIX_MOUNTED_PROBE, Z_SAFE_HOMING)
      // Se o seu home de Z (M206) estiver bem calibrado, habilite para estabelecer
      // o Z0 do probe antes de cada G29
      //#define PROBE_Z0_BEFORE_G29
    #endif

    // Além da grade sondada, continuar a inclinação implícita?
    // O padrão é manter a altura da borda mais próxima.
    //#define EXTRAPOLATE_BEYOND_GRID

    //
    // Subdivisão da grade pelo método Catmull-Rom.
    // Sintetiza pontos intermediários para produzir uma malha mais detalhada.
    //
    //#define ABL_BILINEAR_SUBDIVISION
    #if ENABLED(ABL_BILINEAR_SUBDIVISION)
      // Número de subdivisões entre os pontos sondados
      #define BILINEAR_SUBDIVISIONS 3
    #endif

  #endif

#elif ENABLED(AUTO_BED_LEVELING_UBL)

  //===========================================================================
  //====================== Nivelamento unificado (UBL) ========================
  //===========================================================================

  //#define MESH_EDIT_GFX_OVERLAY   // Exibe uma sobreposição gráfica durante a edição da malha

  #define MESH_INSET 1              // (mm) Define os limites da malha como uma região recuada da mesa
  #define GRID_MAX_POINTS_X 10      // Não use mais que 15 pontos por eixo: limitação da implementação.
  #define GRID_MAX_POINTS_Y GRID_MAX_POINTS_X

  //#define UBL_HILBERT_CURVE       // Usa distribuição de Hilbert para percorrer menos ao sondar vários pontos

  //#define UBL_TILT_ON_MESH_POINTS         // Usa os pontos de malha mais próximos no G29 J, para melhor referência de Z
  //#define UBL_TILT_ON_MESH_POINTS_3POINT  // Usa os pontos mais próximos no G29 J0 (3 pontos)

  #define UBL_MESH_EDIT_MOVES_Z     // Usuários experientes preferem o bico sem movimento
  #define UBL_SAVE_ACTIVE_ON_M500   // Grava a malha ativa no slot atual ao executar M500

  //#define UBL_Z_RAISE_WHEN_OFF_MESH 2.5 // (mm) Correção de altura Z quando o bico está fora da malha

  //#define UBL_MESH_WIZARD         // Executa vários comandos em sequência para obter uma malha completa

  /**
   * Sondagem proibida dentro da região de um obstáculo.
   */
  //#define AVOID_OBSTACLES
  #if ENABLED(AVOID_OBSTACLES)
    #define CLIP_W  23  // (mm) Largura da presilha da mesa, com alguns mm de folga sobre o tamanho real
    #define CLIP_H  14  // (mm) Altura da presilha da mesa, com alguns mm de folga sobre o tamanho real

    // Retângulos de obstáculo, definidos como { X1, Y1, X2, Y2 }
    #define OBSTACLE1 { (X_BED_SIZE) / 4     - (CLIP_W) / 2,                       0, (X_BED_SIZE) / 4     + (CLIP_W) / 2, CLIP_H }     // (mm)
    #define OBSTACLE2 { (X_BED_SIZE) * 3 / 4 - (CLIP_W) / 2,                       0, (X_BED_SIZE) * 3 / 4 + (CLIP_W) / 2, CLIP_H }     // (mm)
    #define OBSTACLE3 { (X_BED_SIZE) / 4     - (CLIP_W) / 2, (Y_BED_SIZE) - (CLIP_H), (X_BED_SIZE) / 4     + (CLIP_W) / 2, Y_BED_SIZE } // (mm)
    #define OBSTACLE4 { (X_BED_SIZE) * 3 / 4 - (CLIP_W) / 2, (Y_BED_SIZE) - (CLIP_H), (X_BED_SIZE) * 3 / 4 + (CLIP_W) / 2, Y_BED_SIZE } // (mm)

    // A grade sondada precisa ser recuada para o G29 J. Isso não é problema, pois
    // ela só é usada para calcular uma transformação linear da própria malha.
    #define G29J_MESH_TILT_MARGIN ((CLIP_H) + 1)    // (mm)
  #endif

#elif ENABLED(MESH_BED_LEVELING)

  //===========================================================================
  //================================= Malha ===================================
  //===========================================================================

  #define MESH_INSET 10          // (mm) Define os limites da malha como uma região recuada da mesa
  #define GRID_MAX_POINTS_X 3
  #define GRID_MAX_POINTS_Y GRID_MAX_POINTS_X

  //#define MESH_G28_REST_ORIGIN // Após referenciar todos os eixos ('G28' ou 'G28 XYZ'), deixa o Z em Z_MIN_POS

#endif // BED_LEVELING

/**
 * Adiciona um submenu de nivelamento de mesa para ABL ou MBL.
 * Inclui um procedimento guiado se a sondagem manual estiver habilitada.
 */
//#define LCD_BED_LEVELING

#if ENABLED(LCD_BED_LEVELING)
  #define MESH_EDIT_Z_STEP  0.025 // (mm) Tamanho do passo ao sondar o eixo Z manualmente.
  #define LCD_PROBE_Z_RANGE 4     // (mm) Faixa de Z, centrada em Z_MIN_POS, para o ajuste pelo LCD
  //#define MESH_EDIT_MENU        // Adiciona um menu para editar os pontos da malha
#endif

// Adiciona um item de menu para percorrer os cantos da mesa no ajuste manual
//#define LCD_BED_TRAMMING

#if ENABLED(LCD_BED_TRAMMING)
  #define BED_TRAMMING_INSET_LFRB { 30, 30, 30, 30 } // (mm) Recuos: esquerda, frente, direita, fundo
  #define BED_TRAMMING_HEIGHT      0.0        // (mm) Altura Z do bico nos pontos de ajuste
  #define BED_TRAMMING_Z_HOP       4.0        // (mm) Subida de Z entre os pontos
  //#define BED_TRAMMING_INCLUDE_CENTER       // Vai ao centro depois do último canto
  //#define BED_TRAMMING_USE_PROBE
  #if ENABLED(BED_TRAMMING_USE_PROBE)
    #define BED_TRAMMING_PROBE_TOLERANCE 0.1  // (mm)
    #define BED_TRAMMING_VERIFY_RAISED        // Depois que o ajuste dispara o probe, sonda de novo para conferir
    //#define BED_TRAMMING_AUDIO_FEEDBACK
  #endif

  /**
   * Ordem do nivelamento pelos cantos
   *
   * Defina 2 ou 4 pontos. Com 2 pontos, o 3º é o centro da borda oposta.
   *
   *  LF  esquerda-frente    RF  direita-frente
   *  LB  esquerda-fundo     RB  direita-fundo
   *
   * Exemplos:
   *
   *      Padrão         {LF,RB,LB,RF}         {LF,RF}           {LB,LF}
   *  LB --------- RB   LB --------- RB    LB --------- RB   LB --------- RB
   *  |  4       3  |   | 3         2 |    |     <3>     |   | 1           |
   *  |             |   |             |    |             |   |          <3>|
   *  |  1       2  |   | 1         4 |    | 1         2 |   | 2           |
   *  LF --------- RF   LF --------- RF    LF --------- RF   LF --------- RF
   */
  #define BED_TRAMMING_LEVELING_ORDER { LF, RF, RB, LB }
#endif

// @section homing

// O centro da mesa fica em (X=0, Y=0)
//#define BED_CENTER_AT_0_0

// Define a posição de home manualmente. Deixe indefinido para ajuste automático.
// Em DELTA, é o centro-topo do volume cartesiano de impressão.
//#define MANUAL_X_HOME_POS 0
//#define MANUAL_Y_HOME_POS 0
//#define MANUAL_Z_HOME_POS 0
//#define MANUAL_I_HOME_POS 0
//#define MANUAL_J_HOME_POS 0
//#define MANUAL_K_HOME_POS 0
//#define MANUAL_U_HOME_POS 0
//#define MANUAL_V_HOME_POS 0
//#define MANUAL_W_HOME_POS 0

/**
 * Use o "Z Safe Homing" para evitar referenciar o Z com o probe fora da mesa.
 *
 * - Move o probe (ou o bico) para um ponto XY definido antes de referenciar o Z.
 * - Só permite referenciar o Z quando as posições XY são conhecidas e confiáveis.
 * - Se os drivers dormirem, pode ser necessário referenciar XY de novo antes do Z.
 */
#define Z_SAFE_HOMING   // Exigido ao referenciar Z pelo probe: vai ao centro da mesa antes

#if ENABLED(Z_SAFE_HOMING)
  #define Z_SAFE_HOMING_X_POINT X_CENTER  // (mm) Ponto X para referenciar o Z
  #define Z_SAFE_HOMING_Y_POINT Y_CENTER  // (mm) Ponto Y para referenciar o Z
  //#define Z_SAFE_HOMING_POINT_ABSOLUTE  // Ignora os offsets de home (M206) nessa posição
#endif

// Velocidades de homing (linear=mm/min, rotacional=°/min)
#define HOMING_FEEDRATE_MM_M { (50*60), (50*60), (4*60) }

// Edita as velocidades de homing com M210 e pelos menus do MarlinUI
//#define EDITABLE_HOMING_FEEDRATE

// Valida que os fins de curso realmente dispararam nos movimentos de homing
#define VALIDATE_HOMING_ENDSTOPS

// @section calibrate

/**
 * Compensação de distorção (skew) da mesa
 *
 * Este recurso corrige desalinhamento nos eixos XYZ.
 *
 * Para obter a distorção no plano XY:
 *  1. Imprima um quadrado de teste (ex.: https://www.thingiverse.com/thing:2563185)
 *  2. Para XY_DIAG_AC meça a diagonal de A a C
 *  3. Para XY_DIAG_BD meça a diagonal de B a D
 *  4. Para XY_SIDE_AD meça a aresta de A a D
 *
 * O Marlin calcula os fatores de distorção a partir dessas medidas.
 * Eles também podem ser calculados e definidos manualmente:
 *
 *  - Calcule AB     : SQRT(2*AC*AC+2*BD*BD-4*AD*AD)/2
 *  - XY_SKEW_FACTOR : TAN(PI/2-ACOS((AC*AC-AB*AB-AD*AD)/(2*AB*AD)))
 *
 * Se quiser, repita o mesmo procedimento para XZ e YZ.
 * Use estes diagramas como referência:
 *
 *    Y                     Z                     Z
 *    ^     B-------C       ^     B-------C       ^     B-------C
 *    |    /       /        |    /       /        |    /       /
 *    |   /       /         |   /       /         |   /       /
 *    |  A-------D          |  A-------D          |  A-------D
 *    +-------------->X     +-------------->X     +-------------->Y
 *     XY_SKEW_FACTOR        XZ_SKEW_FACTOR        YZ_SKEW_FACTOR
 */
//#define SKEW_CORRECTION

#if ENABLED(SKEW_CORRECTION)
  // Informe aqui todas as medidas de comprimento:
  #define XY_DIAG_AC 282.8427124746
  #define XY_DIAG_BD 282.8427124746
  #define XY_SIDE_AD 200

  // Ou defina o fator de distorção XY diretamente:
  //#define XY_SKEW_FACTOR 0.0

  //#define SKEW_CORRECTION_FOR_Z
  #if ENABLED(SKEW_CORRECTION_FOR_Z)
    #define XZ_DIAG_AC 282.8427124746
    #define XZ_DIAG_BD 282.8427124746
    #define YZ_DIAG_AC 282.8427124746
    #define YZ_DIAG_BD 282.8427124746
    #define YZ_SIDE_AD 200

    // Ou defina os fatores de distorção de Z diretamente:
    //#define XZ_SKEW_FACTOR 0.0
    //#define YZ_SKEW_FACTOR 0.0
  #endif

  // Habilite esta opção para ajustar a distorção em tempo de execução com M852
  //#define SKEW_CORRECTION_GCODE
#endif

//=============================================================================
//========================== Recursos adicionais ==============================
//=============================================================================

// @section eeprom

/**
 * EEPROM
 *
 * Armazenamento persistente, que preserva ajustes configuráveis entre reinícios.
 *
 *   M500 - Grava os ajustes na EEPROM.
 *   M501 - Lê os ajustes da EEPROM. (ou seja, descarta alterações não salvas)
 *   M502 - Restaura os ajustes de "fábrica". (Siga com M500 para inicializar a EEPROM.)
 */
#define EEPROM_SETTINGS       // Armazenamento persistente com M500 e M501
//#define DISABLE_M503        // Economiza ~2700 bytes de flash. Não desabilite em versão final!
#define EEPROM_CHITCHAT       // Dá retorno nos comandos de EEPROM. Desabilite para economizar flash.
#define EEPROM_BOOT_SILENT    // Mantém o M503 silencioso, reportando só erros na primeira carga
#if ENABLED(EEPROM_SETTINGS)
  //#define EEPROM_AUTO_INIT  // Inicializa a EEPROM automaticamente em caso de erro.
  //#define EEPROM_INIT_NOW   // Inicializa a EEPROM no primeiro boot após uma nova compilação.
#endif

// @section host

//
// Host Keepalive
//
// Quando habilitado, o Marlin envia ao host uma mensagem de status "ocupado"
// a cada poucos segundos, enquanto não puder aceitar comandos.
//
#define HOST_KEEPALIVE_FEATURE        // Desabilite se o seu host não lida bem com mensagens de keepalive
#define DEFAULT_KEEPALIVE_INTERVAL 2  // Segundos entre as mensagens de "ocupado". Ajuste com M113.
#define BUSY_WHILE_HEATING            // Alguns hosts exigem mensagens de "ocupado" até durante o aquecimento

// @section units

//
// Suporte ao modo em polegadas (G20/G21)
//
//#define INCH_MODE_SUPPORT

//
// Suporte ao M149, que define as unidades de temperatura
//
//#define TEMPERATURE_UNITS_SUPPORT

// @section temperature presets

//
// Constantes de pré-aquecimento - até 10 são suportadas sem alterações no código
//
#define PREHEAT_1_LABEL       "PLA"
#define PREHEAT_1_TEMP_HOTEND 180
#define PREHEAT_1_TEMP_BED     70
#define PREHEAT_1_TEMP_CHAMBER 35
#define PREHEAT_1_FAN_SPEED     0 // Valor de 0 a 255

#define PREHEAT_2_LABEL       "ABS"
#define PREHEAT_2_TEMP_HOTEND 240
#define PREHEAT_2_TEMP_BED    110
#define PREHEAT_2_TEMP_CHAMBER 35
#define PREHEAT_2_FAN_SPEED     0 // Valor de 0 a 255

/**
 * @section nozzle park
 *
 * Estacionamento do bico
 *
 * Estaciona o bico na posição XYZ informada, em ociosidade ou no G27.
 *
 * O parâmetro "P" controla a ação aplicada ao eixo Z:
 *
 *    P0  (Padrão) Se o Z estiver abaixo da altura de estacionamento, levanta o bico.
 *    P1  Sempre levanta o bico até a altura de estacionamento.
 *    P2  Levanta o bico pelo valor de estacionamento, limitado a Z_MAX_POS.
 */
#define NOZZLE_PARK_FEATURE

#if ENABLED(NOZZLE_PARK_FEATURE)
  // Informe a posição de estacionamento como { X, Y, Z_subida }
  #define NOZZLE_PARK_POINT { (X_MIN_POS + 10), (Y_MAX_POS - 10), 20 }
  #define NOZZLE_PARK_MOVE          0   // Movimento: 0 = XY, 1 = só X, 2 = só Y, 3 = X antes de Y, 4 = Y antes de X
  #define NOZZLE_PARK_Z_RAISE_MIN   2   // (mm) Sempre levanta o Z pelo menos esta distância
  #define NOZZLE_PARK_XY_FEEDRATE 100   // (mm/s) Velocidade dos eixos X e Y (usada também no eixo Z de delta)
  #define NOZZLE_PARK_Z_FEEDRATE    5   // (mm/s) Velocidade do eixo Z (não usada em impressoras delta)
#endif

/**
 * @section nozzle clean
 *
 * Recurso de limpeza do bico
 *
 * Adiciona o comando G12, que executa um processo de limpeza do bico.
 *
 * Parâmetros:
 *   P  Padrão
 *   S  Passadas / repetições
 *   T  Triângulos (só no P1)
 *
 * Padrões:
 *   P0  Linha reta (padrão). Este processo exige um material tipo esponja
 *       numa posição fixa da mesa. O "S" especifica as passadas (ou seja,
 *       movimentos de ida e volta) entre os pontos inicial e final.
 *
 *   P1  Padrão ziguezague entre (X0, Y0) e (X1, Y1). O "T" especifica o número
 *       de triângulos do ziguezague e o "S" define o número de passadas.
 *       Os ziguezagues são feitos na dimensão mais estreita.
 *       Por exemplo, "G12 P1 S1 T3" executa:
 *
 *          --
 *         |  (X0, Y1) |     /\        /\        /\     | (X1, Y1)
 *         |           |    /  \      /  \      /  \    |
 *       A |           |   /    \    /    \    /    \   |
 *         |           |  /      \  /      \  /      \  |
 *         |  (X0, Y0) | /        \/        \/        \ | (X1, Y0)
 *          --         +--------------------------------+
 *                       |________|_________|_________|
 *                           T1        T2        T3
 *
 *   P2  Padrão circular com o centro em NOZZLE_CLEAN_CIRCLE_MIDDLE.
 *       O "R" especifica o raio e o "S" a quantidade de passadas.
 *       Antes de começar, o bico vai para NOZZLE_CLEAN_START_POINT.
 *
 *   Ressalva: o Z final deve ser igual ao Z inicial.
 */
//#define NOZZLE_CLEAN_FEATURE

#if ENABLED(NOZZLE_CLEAN_FEATURE)
  #define NOZZLE_CLEAN_PATTERN_LINE     // Disponibiliza o 'G12 P0' - padrão linear simples
  #define NOZZLE_CLEAN_PATTERN_ZIGZAG   // Disponibiliza o 'G12 P1' - padrão em ziguezague
  #define NOZZLE_CLEAN_PATTERN_CIRCLE   // Disponibiliza o 'G12 P2' - padrão circular

  // Padrão usado quando o 'P' não é informado no G12. Um dos habilitados acima.
  #define NOZZLE_CLEAN_DEFAULT_PATTERN 0

  #define NOZZLE_CLEAN_STROKES     12   // Número padrão de repetições do padrão

  #if ENABLED(NOZZLE_CLEAN_PATTERN_ZIGZAG)
    #define NOZZLE_CLEAN_TRIANGLES  3   // Número padrão de triângulos
  #endif

  // Informe as posições de cada ferramenta como { { X, Y, Z }, { X, Y, Z } }
  // Sistema de hotend duplo pode usar { {  -20, (Y_BED_SIZE / 2), (Z_MIN_POS + 1) },  {  420, (Y_BED_SIZE / 2), (Z_MIN_POS + 1) }}
  #define NOZZLE_CLEAN_START_POINT { {  30, 30, (Z_MIN_POS + 1) } }
  #define NOZZLE_CLEAN_END_POINT   { { 100, 60, (Z_MIN_POS + 1) } }

  #if ENABLED(NOZZLE_CLEAN_PATTERN_CIRCLE)
    #define NOZZLE_CLEAN_CIRCLE_RADIUS 6.5                      // (mm) Raio do padrão circular
    #define NOZZLE_CLEAN_CIRCLE_FN 10                           // Número de segmentos do círculo
    #define NOZZLE_CLEAN_CIRCLE_MIDDLE NOZZLE_CLEAN_START_POINT // Ponto central do círculo
  #endif

  // Move o bico de volta à posição inicial depois da limpeza
  #define NOZZLE_CLEAN_GOBACK

  // Para estação de purga/limpeza sempre na altura do pórtico (portanto sem movimento de Z)
  //#define NOZZLE_CLEAN_NO_Z

  // Para estação de purga/limpeza montada no eixo X
  //#define NOZZLE_CLEAN_NO_Y

  // Exige temperatura mínima do hotend para a limpeza
  #define NOZZLE_CLEAN_MIN_TEMP 170
  //#define NOZZLE_CLEAN_HEATUP       // Aquece o bico em vez de pular a limpeza

  // Script de G-code explícito de limpeza, aplicado a um G12 sem argumentos.
  //#define WIPE_SEQUENCE_COMMANDS "G1 X-17 Y25 Z10 F4000\nG1 Z1\nM114\nG1 X-17 Y25\nG1 X-17 Y95\nG1 X-17 Y25\nG1 X-17 Y95\nG1 X-17 Y25\nG1 X-17 Y95\nG1 X-17 Y25\nG1 X-17 Y95\nG1 X-17 Y25\nG1 X-17 Y95\nG1 X-17 Y25\nG1 X-17 Y95\nG1 Z15\nM400\nG0 X-10.0 Y-9.0"

#endif

// @section host

/**
 * Temporizador de impressão
 *
 * Inicia e para automaticamente o temporizador nos comandos M104/M109/M140/M190/M141/M191.
 * O temporizador só é parado se a temperatura alvo da mesa/câmara estiver abaixo
 * de BED_MINTEMP/CHAMBER_MINTEMP.
 *
 *   M104 (bico, sem esperar)    - temp alta = nada,          temp baixa = para o timer
 *   M109 (bico, esperando)      - temp alta = inicia o timer, temp baixa = para o timer
 *   M140 (mesa, sem esperar)    - temp alta = nada,          temp baixa = para o timer
 *   M190 (mesa, esperando)      - temp alta = inicia o timer, temp baixa = nada
 *   M141 (câmara, sem esperar)  - temp alta = nada,          temp baixa = para o timer
 *   M191 (câmara, esperando)    - temp alta = inicia o timer, temp baixa = nada
 *
 * Para M104/M109, "temp alta" é qualquer valor acima de EXTRUDE_MINTEMP / 2.
 * Para M140/M190, é qualquer valor acima de BED_MINTEMP.
 * Para M141/M191, é qualquer valor acima de CHAMBER_MINTEMP.
 *
 * O temporizador também pode ser controlado pelos comandos:
 *
 *   M75 - Inicia o temporizador de impressão
 *   M76 - Pausa o temporizador
 *   M77 - Para o temporizador
 */
#define PRINTJOB_TIMER_AUTOSTART

// @section stats

/**
 * Contador de impressões
 *
 * Acompanha dados estatísticos, como:
 *
 *  - Total de impressões
 *  - Total de impressões bem-sucedidas
 *  - Total de impressões que falharam
 *  - Tempo total imprimindo
 *
 * Veja as estatísticas atuais com M78.
 */
//#define PRINTCOUNTER
#if ENABLED(PRINTCOUNTER)
  #define PRINTCOUNTER_SAVE_INTERVAL 60 // (minutos) Intervalo de gravação na EEPROM durante a impressão. 0 grava só no fim.
#endif

// @section security

/**
 * Senha
 *
 * Define uma senha numérica para a impressora, que pode ser pedida:
 *
 *  - Quando a impressora liga
 *  - Ao abrir o menu 'Imprimir da mídia'
 *  - Quando a impressão pelo SD termina ou é abortada
 *
 * Os seguintes G-codes podem ser usados:
 *
 *  M510 - Bloqueia a impressora. Impede todos os comandos exceto o M511.
 *  M511 - Desbloqueia a impressora.
 *  M512 - Define, altera e remove a senha.
 *
 * Se você esquecer a senha e ficar bloqueado, será preciso regravar o firmware
 * com o recurso desabilitado, zerar a EEPROM e (opcionalmente) regravar o
 * firmware de novo com o recurso habilitado.
 */
//#define PASSWORD_FEATURE
#if ENABLED(PASSWORD_FEATURE)
  #define PASSWORD_LENGTH 4                 // (nº) Quantidade de dígitos (1-9). Recomenda-se 3 ou 4
  #define PASSWORD_ON_STARTUP
  #define PASSWORD_UNLOCK_GCODE             // Desbloqueia com M511 P<senha>. Desabilite para impedir ataque de força bruta.
  #define PASSWORD_CHANGE_GCODE             // Altera a senha com M512 P<antiga> S<nova>.
  //#define PASSWORD_ON_SD_PRINT_MENU       // Isso não impede a execução de G-codes
  //#define PASSWORD_AFTER_SD_PRINT_END
  //#define PASSWORD_AFTER_SD_PRINT_ABORT
  //#include "Configuration_Secure.h"       // Arquivo externo com PASSWORD_DEFAULT_VALUE
#endif

// @section media

/**
 * CARTÃO SD
 *
 * O suporte a cartão SD vem desabilitado por padrão. Se a sua placa tem slot
 * de SD, você precisa descomentar a opção abaixo, senão não funciona.
 */
//#define SDSUPPORT

/**
 * CARTÃO SD: HABILITAR CRC
 *
 * Usa verificação por CRC e novas tentativas na comunicação com o SD.
 */
#if ENABLED(SDSUPPORT)
  //#define SD_CHECK_AND_RETRY
#endif

// @section interface

/**
 * IDIOMA DO LCD
 *
 * Seleciona o idioma exibido no LCD. Estes idiomas estão disponíveis:
 *
 *   en, an, bg, ca, cz, da, de, el, el_CY, es, eu, fi, fr, gl, hg, hr, hu, id, it,
 *   jp_kana, ko_KR, nl, pl, pt, pt_br, ro, ru, sk, sv, tr, uk, vi, zh_CN, zh_TW
 *
 * (pt_br = português do Brasil)
 *
 * :{ 'en':'English', 'an':'Aragonese', 'bg':'Bulgarian', 'ca':'Catalan', 'cz':'Czech', 'da':'Danish', 'de':'German', 'el':'Greek (Greece)', 'el_CY':'Greek (Cyprus)', 'es':'Spanish', 'eu':'Basque-Euskera', 'fi':'Finnish', 'fr':'French', 'gl':'Galician', 'hg':'Hinglish (Hindi-Latin)', 'hr':'Croatian', 'hu':'Hungarian', 'id':'Indonesian', 'it':'Italian', 'jp_kana':'Japanese', 'ko_KR':'Korean (South Korea)', 'nl':'Dutch', 'pl':'Polish', 'pt':'Portuguese', 'pt_br':'Portuguese (Brazilian)', 'ro':'Romanian', 'ru':'Russian', 'sk':'Slovak', 'sv':'Swedish', 'tr':'Turkish', 'uk':'Ukrainian', 'vi':'Vietnamese', 'zh_CN':'Chinese (Simplified)', 'zh_TW':'Chinese (Traditional)' }
 */
#define LCD_LANGUAGE en

/**
 * Conjunto de caracteres do LCD
 *
 * Obs.: esta opção NÃO se aplica a displays gráficos.
 *
 * Todos os LCDs de caractere fornecem ASCII mais uma destas
 * extensões de idioma:
 *
 *  - JAPANESE ... a mais comum
 *  - WESTERN  ... com mais caracteres acentuados
 *  - CYRILLIC ... para o idioma russo
 *
 * Para descobrir qual extensão está instalada no seu controlador:
 *
 *  - Compile e grave com LCD_LANGUAGE definido como 'test'
 *  - Clique no controlador para ver o menu do LCD
 *  - O LCD vai exibir texto em japonês, ocidental ou cirílico
 *
 * Veja https://marlinfw.org/docs/development/lcd_language.html
 *
 * :['JAPANESE', 'WESTERN', 'CYRILLIC']
 */
#define DISPLAY_CHARSET_HD44780 JAPANESE

/**
 * Estilo da tela de informações (0:Clássico, 1:Průša, 2:CNC)
 *
 * :[0:'Classic', 1:'Průša', 2:'CNC']
 */
#define LCD_INFO_SCREEN_STYLE 0

/**
 * Itens de menu do LCD
 *
 * Desabilita todos os menus e exibe apenas a tela de status, ou apenas remove
 * alguns itens supérfluos para recuperar espaço.
 */
//#define NO_LCD_MENUS
//#define SLIM_LCD_MENUS

//
// AJUSTES DO ENCODER
//
// Esta opção sobrepõe o número padrão de pulsos do encoder necessários para
// produzir um passo. Deve ser aumentado em encoders de alta resolução.
//
//#define ENCODER_PULSES_PER_STEP 4

//
// Use esta opção para sobrepor o número de sinais de passo necessários para
// mover entre o item anterior e o próximo do menu.
//
//#define ENCODER_STEPS_PER_MENU_ITEM 1

/**
 * Opções de sentido do encoder
 *
 * Teste primeiro o comportamento do seu encoder com as duas opções desabilitadas.
 *
 *  Edição de valor E navegação invertidas? Habilite REVERSE_ENCODER_DIRECTION.
 *  Só a navegação invertida?               Habilite REVERSE_MENU_DIRECTION.
 *  Só a edição de valor invertida?         Habilite AS DUAS opções.
 */

//
// Esta opção inverte o sentido do encoder em todos os lugares.
//
//  Habilite se girar no SENTIDO HORÁRIO faz os valores DIMINUÍREM
//
//#define REVERSE_ENCODER_DIRECTION

//
// Esta opção inverte o sentido do encoder na navegação dos menus do LCD.
//
//  Se o sentido horário normalmente desce, passa a subir.
//  Se o sentido horário normalmente sobe, passa a descer.
//
//#define REVERSE_MENU_DIRECTION

//
// Esta opção inverte o sentido do encoder na tela de seleção.
//
//  Se o sentido horário normalmente vai para a esquerda, passa a ir para a direita.
//  Se normalmente vai para a direita, passa a ir para a esquerda.
//
//#define REVERSE_SELECT_DIRECTION

//
// Filtro de ruído (EMI) do encoder
//
// Esta opção aumenta a amostragem do encoder para filtrar cliques fantasmas
// causados por interferência eletromagnética.
//
//#define ENCODER_NOISE_FILTER
#if ENABLED(ENCODER_NOISE_FILTER)
  #define ENCODER_SAMPLES 10
#endif

//
// Homing de eixo individual
//
// Adiciona itens de homing por eixo (Home X, Home Y e Home Z) ao menu do LCD.
//
//#define INDIVIDUAL_AXIS_HOMING_MENU
//#define INDIVIDUAL_AXIS_HOMING_SUBMENU

//
// ALTO-FALANTE / BUZZER
//
// Se você tem um alto-falante capaz de produzir tons, habilite aqui.
// Por padrão o Marlin assume um buzzer de frequência fixa.
//
//#define SPEAKER

//
// Duração e frequência do som de retorno da interface.
// Zere estes valores para desabilitar o retorno sonoro nos menus do LCD.
//
// Obs.: teste a saída de áudio com o G-code:
//  M300 S<frequência Hz> P<duração ms>
//
//#define LCD_FEEDBACK_FREQUENCY_DURATION_MS 2
//#define LCD_FEEDBACK_FREQUENCY_HZ 5000

//
// Tamanho da fila de tons, usada para que os bipes não bloqueiem a execução.
// O padrão é 4, ou sobreponha aqui. Custa 4 bytes de SRAM por entrada.
//
//#define TONE_QUEUE_LENGTH 4

//
// Uma sequência de tons tocada na inicialização, em pares de tom (Hz) e duração (ms).
// Silêncio entre os tons.
//
//#define STARTUP_TUNE { 698, 300, 0, 50, 523, 50, 0, 25, 494, 50, 0, 25, 523, 100, 0, 50, 554, 300, 0, 100, 523, 300 }

/**
 * ===========================================================================
 *  NOTA SOBRE O CATÁLOGO DE DISPLAYS, A PARTIR DAQUI
 * ===========================================================================
 * As seções seguintes são um catálogo de controladores de LCD: centenas de
 * linhas que são, em sua maioria, nomes de produto, números de modelo e URLs —
 * que não se traduzem. Aqui foram traduzidas as NOTAS e instruções (requisitos
 * de biblioteca, avisos de fiação, passos de gravação), mantendo intactos os
 * nomes comerciais e os endereços.
 *
 * Para esta máquina, nada deste catálogo está ativo: a MKS TFT24 opera em modo
 * serial, pela SERIAL_PORT_2, e não como display nativo do Marlin.
 * ===========================================================================
 */
//=============================================================================
//=================== Selecao de LCD / Controlador ===========================
//===================      (LCDs de caractere)      ===========================
//=============================================================================
// @section lcd

//
// RepRapDiscount Smart Controller.
// https://reprap.org/wiki/RepRapDiscount_Smart_Controller
//
// Obs.: normalmente vendido com placa branca.
//
//#define REPRAP_DISCOUNT_SMART_CONTROLLER

//
// GT2560 (YHCB2004) LCD Display
//
// Exige as bibliotecas softwarewire de Testato/Koepel e a
// LiquidCrystal_AIP31068 de Andriy Golovnya.
//
//#define YHCB2004

//
// Original RADDS LCD Display+Encoder+SDCardReader
// https://web.archive.org/web/20200719145306/doku.radds.org/dokumentation/lcd-display/
//
//#define RADDS_DISPLAY

//
// ULTIMAKER Controller.
//
//#define ULTIMAKERCONTROLLER

//
// ULTIPANEL as seen on Thingiverse.
//
//#define ULTIPANEL

//
// PanelOne from T3P3 (via RAMPS 1.4 AUX2/AUX3)
// https://reprap.org/wiki/PanelOne
//
//#define PANEL_ONE

//
// GADGETS3D G3D LCD/SD Controller
// https://reprap.org/wiki/RAMPS_1.3/1.4_GADGETS3D_Shield_with_Panel
//
// Obs.: normalmente vendido com placa azul.
//
//#define G3D_PANEL

//
// RigidBot Panel V1.0
//
//#define RIGIDBOT_PANEL

//
// Makeboard 3D Printer Parts 3D Printer Mini Display 1602 Mini Controller
// https://www.aliexpress.com/item/32765887917.html
//
//#define MAKEBOARD_MINI_2_LINE_DISPLAY_1602

/**
 * ANET and Tronxy 20x4 Controller
 * LCD2004 display with 5 analog buttons.
 *
 * NOTE: Requires ADC_KEYPAD_PIN to be assigned to an analog pin.
 * This LCD is known to be susceptible to electrical interference which
 * scrambles the display. Press any button to clear it up.
 */
//#define ZONESTAR_LCD

//
// LCD generico de caractere 16x2, 16x4, 20x2 ou 20x4.
//
//#define ULTRA_LCD

//=============================================================================
//=================== Selecao de LCD / Controlador ===========================
//============  (LCDs por I2C e por registrador de deslocamento)  =============
//=============================================================================

//
// TIPO DE CONTROLADOR: I2C
//
// Obs.: estes controladores exigem a instalacao da biblioteca LiquidCrystal_I2C
// library. For more info: https://github.com/kiyoshigawa/LiquidCrystal_I2C
//

//
// Elefu RA Board Control Panel
// https://web.archive.org/web/20140823033947/www.elefu.com/index.php?route=product/product&product_id=53
//
//#define RA_CONTROL_PANEL

//
// Sainsmart (YwRobot) LCD Displays
//
// Estes exigem a biblioteca LiquidCrystal_I2C:
//   https://github.com/MarlinFirmware/New-LiquidCrystal
//   https://github.com/fmalpartida/New-LiquidCrystal/wiki
//
//#define LCD_SAINSMART_I2C_1602
//#define LCD_SAINSMART_I2C_2004

//
// Generic LCM1602 LCD adapter
//
//#define LCM1602

//
// PANELOLU2 LCD with status LEDs,
// separate encoder and click inputs.
//
// Obs.: este controlador exige a biblioteca LiquidTWI2 v1.2.3 ou posterior.
// For more info: https://github.com/lincomatic/LiquidTWI2
//
// Note: The PANELOLU2 encoder click input can either be directly connected to
// a pin (if BTN_ENC defined to != -1) or read through I2C (when BTN_ENC == -1).
//
//#define LCD_I2C_PANELOLU2

//
// Panucatt VIKI LCD with status LEDs,
// integrated click & L/R/U/D buttons, separate encoder inputs.
//
//#define LCD_I2C_VIKI

//
// TIPO DE CONTROLADOR: paineis com registrador de deslocamento
//

//
// 2-wire Non-latching LCD SR from https://github.com/fmalpartida/New-LiquidCrystal/wiki/schematics#user-content-ShiftRegister_connection
// LCD configuration: https://reprap.org/wiki/SAV_3D_LCD
//
//#define SAV_3DLCD

//
// 3-wire SR LCD with strobe using 74HC4094
// https://github.com/mikeshub/SailfishLCD
// Usa o codigo diretamente do Sailfish
//
//#define FF_INTERFACEBOARD

//
// LCD e interface da MightyBoard
//
//#define MIGHTYBOARD_LCD
#if ENABLED(MIGHTYBOARD_LCD)
  //#define MIGHTYBOARD_DEBUG               // Lightweight debug output for buttons and encoder
  //#define MIGHTYBOARD_DISABLE_ENC_PULLUP  // Enable if the encoder button is unreliable
  //#define MIGHTYBOARD_BUTTON_PULLUPS      // Enable if other buttons are unreliable
  //#define MIGHTYBOARD_BACK_STATUS_BUTTONS // Use LEFT/RIGHT buttons for Back / Home.
                                            // Otherwise they act like encoder down / up.
#endif

//
// Painel TFT GLCD com a interface do Marlin
// Painel ligado a placa principal por SPI ou I2C.
// See https://github.com/Serhiy-K/TFTGLCDAdapter
//
//#define TFTGLCD_PANEL_SPI
//#define TFTGLCD_PANEL_I2C

//=============================================================================
//=================== Selecao de LCD / Controlador ===========================
//===================        (LCDs graficos)        ===========================
//=============================================================================

//
// TIPO DE CONTROLADOR: grafico 128x64 (DOGM)
//
// IMPORTANTE: a biblioteca U8glib e necessaria para display grafico!
//            https://github.com/olikraus/U8glib_Arduino
//
// OBS.: se o LCD nao responder, pode ser necessario inverter os conectores.
//

//
// RepRapDiscount FULL GRAPHIC Smart Controller
// https://reprap.org/wiki/RepRapDiscount_Full_Graphic_Smart_Controller
//
//#define REPRAP_DISCOUNT_FULL_GRAPHIC_SMART_CONTROLLER

//
// K.3D Full Graphic Smart Controller
//
//#define K3D_FULL_GRAPHIC_SMART_CONTROLLER

//
// ReprapWorld Graphical LCD
// https://reprapworld.com/electronics/3d-printer-modules/autonomous-printing/graphical-lcd-screen-v1-0/
//
//#define REPRAPWORLD_GRAPHICAL_LCD

//
// Ative uma destas se voce tem um Panucatt Devices
// Viki 2.0 ou mini Viki com LCD grafico
// https://www.panucatt.com
//
//#define VIKI2
//#define miniVIKI

//
// Alfawise Ex8 printer LCD marked as WYH L12864 COG
//
//#define WYH_L12864

//
// MakerLab Mini Panel with graphic
// controller and SD support - https://reprap.org/wiki/Mini_panel
//
//#define MINIPANEL

//
// MaKr3d Makr-Panel with graphic controller and SD support.
// https://reprap.org/wiki/MaKrPanel
//
//#define MAKRPANEL

//
// Adafruit ST7565 Full Graphic Controller.
// https://github.com/eboston/Adafruit-ST7565-Full-Graphic-Controller/
//
//#define ELB_FULL_GRAPHIC_CONTROLLER

//
// BQ LCD Smart Controller shipped by
// default with the BQ Hephestos 2 and Witbox 2.
//
//#define BQ_LCD_SMART_CONTROLLER

//
// Cartesio UI
// https://web.archive.org/web/20180605050442/mauk.cc/webshop/cartesio-shop/electronics/user-interface
//
//#define CARTESIO_UI

//
// LCD for Melzi Card with Graphical LCD
//
//#define LCD_FOR_MELZI

//
// Original Ulticontroller from Ultimaker 2 printer with SSD1309 I2C display and encoder
// https://github.com/Ultimaker/Ultimaker2/tree/master/1249_Ulticontroller_Board_(x1)
//
//#define ULTI_CONTROLLER

//
// MKS MINI12864 with graphic controller and SD support
// https://reprap.org/wiki/MKS_MINI_12864
//
//#define MKS_MINI_12864

//
// MKS MINI12864 V3 is an alias for FYSETC_MINI_12864_2_1. Type A/B. NeoPixel RGB Backlight.
//
//#define MKS_MINI_12864_V3

//
// MKS LCD12864A/B with graphic controller and SD support. Follows MKS_MINI_12864 pinout.
// https://www.aliexpress.com/item/33018110072.html
//
//#define MKS_LCD12864A
//#define MKS_LCD12864B

//
// FYSETC variant of the MINI12864 graphic controller with SD support
// https://wiki.fysetc.com/docs/Mini12864Panel
//
//#define FYSETC_MINI_12864_X_X    // Type C/D/E/F. No tunable RGB Backlight by default
//#define FYSETC_MINI_12864_1_2    // Type C/D/E/F. Simple RGB Backlight (always on)
//#define FYSETC_MINI_12864_2_0    // Type A/B. Discreet RGB Backlight
//#define FYSETC_MINI_12864_2_1    // Type A/B. NeoPixel RGB Backlight
//#define FYSETC_GENERIC_12864_1_1 // Larger display with basic ON/OFF backlight.

//
// BigTreeTech Mini 12864 V1.0 / V2.0 is an alias for FYSETC_MINI_12864_2_1. Type A/B. NeoPixel RGB Backlight.
// https://github.com/bigtreetech/MINI-12864
//
//#define BTT_MINI_12864

//
// BEEZ MINI 12864 is an alias for FYSETC_MINI_12864_2_1. Type A/B. NeoPixel RGB Backlight.
//
//#define BEEZ_MINI_12864

//
// Display de fabrica da Creality CR-10 / CR-7 / Ender-3
// https://marlinfw.org/docs/hardware/controllers.html#cr10_stockdisplay
//
// Conecte no EXP1 em placas RAMPS e compativeis.
//
//#define CR10_STOCKDISPLAY

//
// Ender-2 OEM display, a variant of the MKS_MINI_12864
//
//#define ENDER2_STOCKDISPLAY

//
// ANET and Tronxy 128×64 Full Graphics Controller as used on Anet A6
//
//#define ANET_FULL_GRAPHICS_LCD

//
// GUCOCO CTC 128×64 Full Graphics Controller as used on GUCOCO CTC A10S
//
//#define CTC_A10S_A13

//
// AZSMZ 12864 LCD with SD
// https://www.aliexpress.com/item/32837222770.html
//
//#define AZSMZ_12864

//
// Silvergate GLCD controller
// https://github.com/android444/Silvergate
//
//#define SILVER_GATE_GLCD_CONTROLLER

//
// eMotion Tech LCD with SD
// https://www.reprap-france.com/produit/1234568748-ecran-graphique-128-x-64-points-2-1
//
//#define EMOTION_TECH_LCD

//=============================================================================
//============================  Displays OLED  ================================
//=============================================================================

//
// SSD1306 OLED full graphics generic display
//
//#define U8GLIB_SSD1306

//
// SAV OLEd LCD module support using either SSD1306 or SH1106 based LCD modules
//
//#define SAV_3DGLCD
#if ENABLED(SAV_3DGLCD)
  #define U8GLIB_SSD1306
  //#define U8GLIB_SH1106
#endif

//
// TinyBoy2 128x64 OLED / Encoder Panel
//
//#define OLED_PANEL_TINYBOY2

//
// MKS OLED 1.3" 128×64 Full Graphics Controller
// https://reprap.org/wiki/MKS_12864OLED
//
// Display OLED pequeno, mas muito nitido
//
//#define MKS_12864OLED          // Uses the SH1106 controller
//#define MKS_12864OLED_SSD1306  // Uses the SSD1306 controller

//
// Zonestar OLED 128×64 Full Graphics Controller
//
//#define ZONESTAR_12864LCD           // Graphical (DOGM) with ST7920 controller
//#define ZONESTAR_12864OLED          // 1.3" OLED with SH1106 controller
//#define ZONESTAR_12864OLED_SSD1306  // 0.96" OLED with SSD1306 controller

//
// Einstart S OLED SSD1306
//
//#define U8GLIB_SH1106_EINSTART

//
// Overlord OLED display/controller with i2c buzzer and LEDs
//
//#define OVERLORD_OLED

//
// FYSETC OLED 2.42" 128×64 Full Graphics Controller with WS2812 RGB
// Where to find : https://www.aliexpress.com/item/4000345255731.html
//#define FYSETC_242_OLED_12864   // Uses the SSD1309 controller

//
// K.3D SSD1309 OLED 2.42" 128×64 Full Graphics Controller
//
//#define K3D_242_OLED_CONTROLLER   // Software SPI

//=============================================================================
//=================== Displays de interface extensivel ========================
//=============================================================================

/**
 * Display de toque DGUS com DWIN OS. (Escolha um.)
 *
 * ORIGIN (Marlin DWIN_SET)
 *  - Download https://github.com/coldtobi/Marlin_DGUS_Resources
 *  - Copy the downloaded DWIN_SET folder to the SD card.
 *  - Product: https://www.aliexpress.com/item/32993409517.html
 *
 * FYSETC (Supplier default)
 *  - Download https://github.com/FYSETC/FYSTLCD-2.0
 *  - Copy the downloaded SCREEN folder to the SD card.
 *  - Product: https://www.aliexpress.com/item/32961471929.html
 *
 * HIPRECY (Supplier default)
 *  - Download https://github.com/HiPrecy/Touch-Lcd-LEO
 *  - Copy the downloaded DWIN_SET folder to the SD card.
 *
 * MKS (MKS-H43) (Supplier default)
 *  - Download https://github.com/makerbase-mks/MKS-H43
 *  - Copy the downloaded DWIN_SET folder to the SD card.
 *  - Product: https://www.aliexpress.com/item/1005002008179262.html
 *
 * RELOADED (T5UID1)
 *  - Download https://github.com/Neo2003/DGUS-reloaded/releases
 *  - Copy the downloaded DWIN_SET folder to the SD card.
 *
 * IA_CREALITY (T5UID1)
 *  - Download https://github.com/InsanityAutomation/Marlin/raw/CrealityDwin_2.0/TM3D_Combined480272_Landscape_V7.7z
 *  - Copy the downloaded DWIN_SET folder to the SD card.
 *
 * E3S1PRO (T5L)
 *  - Download https://github.com/CrealityOfficial/Ender-3S1/archive/3S1_Plus_Screen.zip
 *  - Copy the downloaded DWIN_SET folder to the SD card.
 *
 * Grave o display com os DGUS Displays for Marlin:
 *  - Format the SD card to FAT32 with an allocation size of 4kb.
 *  - Download files as specified for your type of display.
 *  - Plug the microSD card into the back of the display.
 *  - Boot the display and wait for the update to complete.
 *
 * :[ 'ORIGIN', 'FYSETC', 'HYPRECY', 'MKS', 'RELOADED', 'IA_CREALITY', 'E3S1PRO' ]
 */
//#define DGUS_LCD_UI ORIGIN
#if DGUS_UI_IS(MKS)
  #define USE_MKS_GREEN_UI
#elif DGUS_UI_IS(IA_CREALITY)
  //#define LCD_SCREEN_ROTATE 90          // Portrait Mode or 800x480 displays
  //#define IA_CREALITY_BOOT_DELAY 1500   // (ms)
#endif

//
// LCD for Malyan M200/M300 printers
//
//#define MALYAN_LCD

//
// Touch UI for FTDI EVE (FT800/FT810) displays
// See Configuration_adv.h for all configuration options.
//
//#define TOUCH_UI_FTDI_EVE

//
// Touch-screen LCD for Anycubic Chiron
//
//#define ANYCUBIC_LCD_CHIRON

//
// Touch-screen LCD for Anycubic i3 Mega
//
//#define ANYCUBIC_LCD_I3MEGA
#if ENABLED(ANYCUBIC_LCD_I3MEGA)
  //#define ANYCUBIC_LCD_GCODE_EXT  // Add ".gcode" to menu entries for DGUS clone compatibility
#endif

//
// Touch-screen LCD for Anycubic Vyper
//
//#define ANYCUBIC_LCD_VYPER

//
// Sovol SV-06 Resistive Touch Screen
//
//#define SOVOL_SV06_RTS

//
// 320x240 Nextion 2.8" serial TFT Resistive Touch Screen NX3224T028
//
//#define NEXTION_TFT

//
// Elegoo Neptune 3 Pro / Plus / Max TJC Touch Screen
// Requires a 6x6 (Pro), 7x7 (Plus), or 7x9 (Max) bilinear leveling grid.
//
//#define ELEGOO_NEPTUNE_3_TFT

//
// Third-party or vendor-customized controller interfaces.
// Sources should be installed in 'src/lcd/extui'.
//
//#define EXTENSIBLE_UI

#if ENABLED(EXTENSIBLE_UI)
  //#define EXTUI_LOCAL_BEEPER // Enables use of local Beeper pin with external display
#endif

//=============================================================================
//=============================== Graphical TFTs ==============================
//=============================================================================

/**
 * Specific TFT Model Presets. Enable one of the following options
 * or enable TFT_GENERIC and set sub-options.
 */

//
// 480x320, 3.5", SPI Display with Rotary Encoder from MKS
// Usually paired with MKS Robin Nano V2 & V3
// https://github.com/makerbase-mks/MKS-TFT-Hardware/tree/master/MKS%20TS35
//
//#define MKS_TS35_V2_0

//
// MKS TS24-R V2.1 (2.4" 320x240 ST7789V) as shipped with the MKS DLC32
//
//#define MKS_TS24_R_V2_1

//
// 320x240, 2.4", FSMC Display From MKS
// Usually paired with MKS Robin Nano V1.2
//
//#define MKS_ROBIN_TFT24

//
// 320x240, 2.8", FSMC Display From MKS
// Usually paired with MKS Robin Nano V1.2
//
//#define MKS_ROBIN_TFT28

//
// 320x240, 3.2", FSMC Display From MKS
// Usually paired with MKS Robin Nano V1.2
//
//#define MKS_ROBIN_TFT32

//
// 480x320, 3.5", FSMC Display From MKS
// Usually paired with MKS Robin Nano V1.2
//
//#define MKS_ROBIN_TFT35

//
// 480x272, 4.3", FSMC Display From MKS
//
//#define MKS_ROBIN_TFT43

//
// 320x240, 3.2", FSMC Display From MKS
// Usually paired with MKS Robin
//
//#define MKS_ROBIN_TFT_V1_1R

//
// 480x320, 3.5", FSMC Stock Display from Tronxy
//
//#define TFT_TRONXY_X5SA

//
// 480x320, 3.5", FSMC Stock Display from AnyCubic
//
//#define ANYCUBIC_TFT35

//
// 320x240, 2.8", FSMC Stock Display from Longer/Alfawise
//
//#define LONGER_LK_TFT28

//
// 320x240, 2.8", FSMC Stock Display from ET4
//
//#define ANET_ET4_TFT28

//
// 480x320, 3.5", FSMC Stock Display from ET5
//
//#define ANET_ET5_TFT35

//
// 1024x600, 7", RGB Stock Display with Rotary Encoder from BIQU BX
// https://github.com/bigtreetech/BIQU-BX/tree/master/Hardware
//
//#define BIQU_BX_TFT70

//
// 480x320, 3.5", SPI Stock Display with Rotary Encoder from BIQU B1 SE Series
// https://github.com/bigtreetech/TFT35-SPI/tree/master/v1
//
//#define BTT_TFT35_SPI_V1_0

//
// Generic TFT with detailed options
//
//#define TFT_GENERIC
#if ENABLED(TFT_GENERIC)
  // :[ 'AUTO', 'ST7735', 'ST7789', 'ST7796', 'R61505', 'ILI9328', 'ILI9341', 'ILI9488' ]
  #define TFT_DRIVER AUTO

  // Interface. Enable one of the following options:
  //#define TFT_INTERFACE_FSMC
  //#define TFT_INTERFACE_SPI

  // TFT Resolution. Enable one of the following options:
  //#define TFT_RES_320x240
  //#define TFT_RES_480x272
  //#define TFT_RES_480x320
  //#define TFT_RES_1024x600
#endif

/**
 * TFT UI - User Interface Selection. Enable one of the following options:
 *
 *   TFT_CLASSIC_UI - Emulated DOGM - 128x64 Upscaled
 *   TFT_COLOR_UI   - Marlin Default Menus, Touch Friendly, using full TFT capabilities
 *   TFT_LVGL_UI    - A Modern UI using LVGL
 *
 *   For LVGL_UI also copy the 'assets' folder from the build directory to the
 *   root of your SD card, together with the compiled firmware.
 */
//#define TFT_CLASSIC_UI
//#define TFT_COLOR_UI
//#define TFT_LVGL_UI

#if ENABLED(TFT_COLOR_UI)
  /**
   * TFT Font for Color UI. Choose one of the following:
   *
   * NOTOSANS  - Default font with anti-aliasing. Supports Latin Extended and non-Latin characters.
   * UNIFONT   - Lightweight font, no anti-aliasing. Supports Latin Extended and non-Latin characters.
   * HELVETICA - Lightweight font, no anti-aliasing. Supports Basic Latin (0x0020-0x007F) and Latin-1 Supplement (0x0080-0x00FF) characters only.
   * :['NOTOSANS', 'UNIFONT', 'HELVETICA']
   */
  #define TFT_FONT  NOTOSANS

  /**
   * TFT Theme for Color UI. Choose one of the following or add a new one to 'Marlin/src/lcd/tft/themes' directory
   *
   * BLUE_MARLIN  - Default theme with 'midnight blue' background
   * BLACK_MARLIN - Theme with 'black' background
   * ANET_BLACK   - Theme used for Anet ET4/5
   * :['BLUE_MARLIN', 'BLACK_MARLIN', 'ANET_BLACK']
   */
  #define TFT_THEME BLACK_MARLIN

  //#define TFT_SHARED_IO   // I/O is shared between TFT display and other devices. Disable async data transfer.

  #define COMPACT_MARLIN_BOOT_LOGO  // Use compressed data to save Flash space
#endif

#if ENABLED(TFT_LVGL_UI)
  //#define MKS_WIFI_MODULE // MKS WiFi module
#endif

/**
 * TFT Rotation. Set to one of the following values:
 *
 *   TFT_ROTATE_90,  TFT_ROTATE_90_MIRROR_X,  TFT_ROTATE_90_MIRROR_Y,
 *   TFT_ROTATE_180, TFT_ROTATE_180_MIRROR_X, TFT_ROTATE_180_MIRROR_Y,
 *   TFT_ROTATE_270, TFT_ROTATE_270_MIRROR_X, TFT_ROTATE_270_MIRROR_Y,
 *   TFT_MIRROR_X, TFT_MIRROR_Y, TFT_NO_ROTATION
 *
 * :{ 'TFT_NO_ROTATION':'None', 'TFT_ROTATE_90':'90°', 'TFT_ROTATE_90_MIRROR_X':'90° (Mirror X)', 'TFT_ROTATE_90_MIRROR_Y':'90° (Mirror Y)', 'TFT_ROTATE_180':'180°', 'TFT_ROTATE_180_MIRROR_X':'180° (Mirror X)', 'TFT_ROTATE_180_MIRROR_Y':'180° (Mirror Y)', 'TFT_ROTATE_270':'270°', 'TFT_ROTATE_270_MIRROR_X':'270° (Mirror X)', 'TFT_ROTATE_270_MIRROR_Y':'270° (Mirror Y)', 'TFT_MIRROR_X':'Mirror X', 'TFT_MIRROR_Y':'Mirror Y' }
 */
//#define TFT_ROTATION TFT_NO_ROTATION

//=============================================================================
//============================  Other Controllers  ============================
//=============================================================================

//
// Ender-3 v2 OEM display. A DWIN display with Rotary Encoder.
//
//#define DWIN_CREALITY_LCD           // Creality UI
//#define DWIN_LCD_PROUI              // Pro UI by MRiscoC
//#define DWIN_CREALITY_LCD_JYERSUI   // Jyers UI by Jacob Myers
//#define DWIN_MARLINUI_PORTRAIT      // MarlinUI (portrait orientation)
//#define DWIN_MARLINUI_LANDSCAPE     // MarlinUI (landscape orientation)

#if ENABLED(DWIN_CREALITY_LCD)
  //#define USE_STRING_HEADINGS       // Use string headings for Creality UI instead of images
  //#define USE_STRING_TITLES         // Use string titles for Creality UI instead of images
#endif

//
// Touch Screen Settings
//
//#define TOUCH_SCREEN
#if ENABLED(TOUCH_SCREEN)
  #define BUTTON_DELAY_EDIT      50 // (ms) Button repeat delay for edit screens
  #define BUTTON_DELAY_MENU     250 // (ms) Button repeat delay for menus

  #if ANY(TFT_CLASSIC_UI, TFT_COLOR_UI)
    //#define NO_BACK_MENU_ITEM     // Don't display a top menu item to go back to the parent menu
  #endif

  #define TOUCH_SCREEN_CALIBRATION

  //#define TOUCH_CALIBRATION_X 12316
  //#define TOUCH_CALIBRATION_Y -8981
  //#define TOUCH_OFFSET_X        -43
  //#define TOUCH_OFFSET_Y        257
  //#define TOUCH_ORIENTATION TOUCH_LANDSCAPE

  #if ALL(TOUCH_SCREEN_CALIBRATION, EEPROM_SETTINGS)
    #define TOUCH_CALIBRATION_AUTO_SAVE // Auto save successful calibration values to EEPROM
  #endif

  #if ENABLED(TFT_COLOR_UI)
    //#define SINGLE_TOUCH_NAVIGATION
  #endif
#endif

//
// RepRapWorld REPRAPWORLD_KEYPAD v1.1
// https://reprapworld.com/products/electronics/ramps/keypad_v1_0_fully_assembled/
//
//#define REPRAPWORLD_KEYPAD
#if ENABLED(REPRAPWORLD_KEYPAD)
  //#define REPRAPWORLD_KEYPAD_MOVE_STEP 10.0 // (mm) Distance to move per key-press
#endif

//
// EasyThreeD ET-4000+ with button input and status LED
//
//#define EASYTHREED_UI

//=============================================================================
//=============================== Extra Features ==============================
//=============================================================================

// @section fans

// Set number of user-controlled fans. Disable to use all board-defined fans.
// :[1,2,3,4,5,6,7,8]
//#define NUM_M106_FANS 1

/**
 * Use software PWM to drive the fan, as for the heaters. This uses a very low frequency
 * which is not as annoying as with the hardware PWM. On the other hand, if this frequency
 * is too low, you should also increment SOFT_PWM_SCALE.
 */
//#define FAN_SOFT_PWM

/**
 * Incrementing this by 1 will double the software PWM frequency, affecting heaters, and
 * the fan if FAN_SOFT_PWM is enabled. However, control resolution will be halved for each
 * increment; at zero value, there are 128 effective control positions.
 * :[0,1,2,3,4,5,6,7]
 */
#define SOFT_PWM_SCALE 0

/**
 * If SOFT_PWM_SCALE is set to a value higher than 0, dithering can be used to mitigate the
 * associated resolution loss. If enabled, some of the PWM cycles are stretched so on average
 * the desired duty cycle is attained.
 */
//#define SOFT_PWM_DITHER

// @section extras

// Support for the BariCUDA Paste Extruder
//#define BARICUDA

// @section lights

/**
 * Temperature status LEDs that display the hotend and bed temperature.
 * If all hotends, bed temperature, and target temperature are under 54C
 * the BLUE led is on. Otherwise the RED led is on. (1C hysteresis)
 */
//#define TEMP_STAT_LEDS

// Support for BlinkM/CyzRgb
//#define BLINKM

// Support for PCA9632 PWM LED driver
//#define PCA9632

// Support for PCA9533 PWM LED driver
//#define PCA9533

/**
 * RGB LED / LED Strip Control
 *
 * Enable support for an RGB LED connected to 5V digital pins, or
 * an RGB Strip connected to MOSFETs controlled by digital pins.
 *
 * Adds the M150 command to set the LED (or LED strip) color.
 * If pins are PWM capable (e.g., 4, 5, 6, 11) then a range of
 * luminance values can be set from 0 to 255.
 * For NeoPixel LED an overall brightness parameter is also available.
 *
 *  === CAUTION ===
 *  LED Strips require a MOSFET Chip between PWM lines and LEDs,
 *  as the Arduino cannot handle the current the LEDs will require.
 *  Failure to follow this precaution can destroy your Arduino!
 *
 *  NOTE: A separate 5V power supply is required! The NeoPixel LED needs
 *  more current than the Arduino 5V linear regulator can produce.
 *
 *  Requires PWM frequency between 50 <> 100Hz (Check HAL or variant)
 *  Use FAST_PWM_FAN, if possible, to reduce fan noise.
 */

// LED Type. Enable only one of the following two options:
//#define RGB_LED
//#define RGBW_LED

#if ANY(RGB_LED, RGBW_LED)
  //#define RGB_LED_R_PIN 34
  //#define RGB_LED_G_PIN 43
  //#define RGB_LED_B_PIN 35
  //#define RGB_LED_W_PIN -1
#endif

#if ANY(RGB_LED, RGBW_LED, PCA9632)
  //#define RGB_STARTUP_TEST              // For PWM pins, fade between all colors
  #if ENABLED(RGB_STARTUP_TEST)
    #define RGB_STARTUP_TEST_INNER_MS 10  // (ms) Reduce or increase fading speed
  #endif
#endif

// Support for Adafruit NeoPixel LED driver
//#define NEOPIXEL_LED
#if ENABLED(NEOPIXEL_LED)
  #define NEOPIXEL_TYPE          NEO_GRBW // NEO_GRBW, NEO_RGBW, NEO_GRB, NEO_RBG, etc.
                                          // See https://github.com/adafruit/Adafruit_NeoPixel/blob/master/Adafruit_NeoPixel.h
  //#define NEOPIXEL_PIN                4 // LED driving pin
  //#define NEOPIXEL2_TYPE  NEOPIXEL_TYPE
  //#define NEOPIXEL2_PIN               5
  #define NEOPIXEL_PIXELS              30 // Number of LEDs in the strip. (Longest strip when NEOPIXEL2_SEPARATE is disabled.)
  #define NEOPIXEL_IS_SEQUENTIAL          // Sequential display for temperature change - LED by LED. Disable to change all LEDs at once.
  #define NEOPIXEL_BRIGHTNESS         127 // Initial brightness (0-255)
  //#define NEOPIXEL_STARTUP_TEST         // Cycle through colors at startup

  // Support for second Adafruit NeoPixel LED driver controlled with M150 S1 ...
  //#define NEOPIXEL2_SEPARATE
  #if ENABLED(NEOPIXEL2_SEPARATE)
    #define NEOPIXEL2_PIXELS           15 // Number of LEDs in the second strip
    #define NEOPIXEL2_BRIGHTNESS      127 // Initial brightness (0-255)
    #define NEOPIXEL2_STARTUP_TEST        // Cycle through colors at startup
    #define NEOPIXEL_M150_DEFAULT      -1 // Default strip for M150 without 'S'. Use -1 to set all by default.
  #else
    //#define NEOPIXEL2_INSERIES          // Default behavior is NeoPixel 2 in parallel
  #endif

  // Use some of the NeoPixel LEDs for static (background) lighting
  //#define NEOPIXEL_BKGD_INDEX_FIRST   0 // Index of the first background LED
  //#define NEOPIXEL_BKGD_INDEX_LAST    5 // Index of the last background LED
  //#define NEOPIXEL_BKGD_COLOR         { 255, 255, 255, 0 }  // R, G, B, W
  //#define NEOPIXEL_BKGD_TIMEOUT_COLOR {  25,  25,  25, 0 }  // R, G, B, W
  //#define NEOPIXEL_BKGD_ALWAYS_ON       // Keep the backlight on when other NeoPixels are off
#endif

/**
 * Printer Event LEDs
 *
 * During printing, the LEDs will reflect the printer status:
 *
 *  - Gradually change from blue to violet as the heated bed gets to target temp
 *  - Gradually change from violet to red as the hotend gets to temperature
 *  - Change to white to illuminate work surface
 *  - Change to green once print has finished
 *  - Turn off after the print has finished and the user has pushed a button
 */
#if ANY(BLINKM, RGB_LED, RGBW_LED, PCA9632, PCA9533, NEOPIXEL_LED)
  #define PRINTER_EVENT_LEDS
#endif

// @section servos

/**
 * Number of servos
 *
 * For some servo-related options NUM_SERVOS will be set automatically.
 * Set this manually if there are extra servos needing manual control.
 * Set to 0 to turn off servo support.
 */
#define NUM_SERVOS 1 // Note: Servo index starts with 0 for M280-M282 commands

// (ms) Delay before the next move will start, to give the servo time to reach its target angle.
// 300ms is a good value but you can try less delay.
// If the servo can't reach the requested position, increase it.
#define SERVO_DELAY { 300 }

// Only power servos during movement, otherwise leave off to prevent jitter
#define DEACTIVATE_SERVOS_AFTER_MOVE

// Edit servo angles with M281 and save to EEPROM with M500
//#define EDITABLE_SERVO_ANGLES

// Disable servo with M282 to reduce power consumption, noise, and heat when not in use
//#define SERVO_DETACH_GCODE
