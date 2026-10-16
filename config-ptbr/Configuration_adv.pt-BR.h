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
 * Cópia traduzida de Marlin/Configuration_adv.h. O firmware é compilado a
 * partir do arquivo original; este aqui existe só para leitura e estudo.
 * ===========================================================================
 *
 * Configuration_adv.h
 *
 * Ajustes avançados.
 * Só altere estes se souber exatamente o que está fazendo.
 * Alguns deles podem danificar a sua impressora se ajustados errado!
 *
 * Ajustes básicos ficam em Configuration.h
 */

/**
 * ###########################################################################
 *  ATENÇÃO: TRADUÇÃO INCOMPLETA
 * ###########################################################################
 * Traduzidas apenas as primeiras ~210 linhas (ajustes de ADC, termopar e
 * termistor personalizado). O restante do Configuration_adv.h ainda NÃO foi
 * traduzido.
 *
 * Não use este arquivo como referência completa: ele cobre uma fração pequena
 * do original. Para o que ainda falta, consulte Marlin/Configuration_adv.h.
 *
 * A tradução do Configuration.h, essa sim, está completa e verificada
 * (477 defines conferidos contra o original).
 * ###########################################################################
 */
#define CONFIGURATION_ADV_H_VERSION 02010300

// @section develop

/**
 * Exportação da configuração
 *
 * Exporta a configuração como parte da compilação. (Veja signature.py)
 * Os arquivos de saída são gravados junto com o build (ex.: .pio/build/mega2560).
 *
 * Veja `build_all_examples --ini` como exemplo de arquivamento do config.ini.
 *
 *  1 = marlin_config.json - Dicionário contendo a configuração.
 *      Este arquivo também é gerado para o CONFIGURATION_EMBEDDING.
 *  2 = config.ini - Formato de arquivo para o pré-processamento do PlatformIO.
 *  3 = schema.json - O esquema completo da configuração. (13 = grupos de padrões)
 *  4 = schema.yml - O esquema completo da configuração.
 *  5 = Config.h - Configuração mínima, por demanda popular.
 */
//#define CONFIG_EXPORT 105 // :[1:'JSON', 2:'config.ini', 3:'schema.json', 4:'schema.yml', 5:'Config.h']

//===========================================================================
//========================= Ajustes de temperatura ==========================
//===========================================================================
// @section temperature

/**
 * Tensão de referência do ADC (MKS TinyBee / ESP32)
 *
 * A tensão de referência do ADC varia de placa para placa e pode deslocar a
 * leitura de temperatura em mais de 10 °C. Com os termistores DESCONECTADOS,
 * meça a tensão (VDDA) no 2º pino do conector TH1/TH2 e coloque o valor aqui.
 * O valor abaixo é o padrão de pins_MKS_TINYBEE.h — ajuste após medir.
 */
#define ADC_REFERENCE_VOLTAGE 2.565


/**
 * Sensores de termopar são bastante sensíveis a ruído. Qualquer ruído induzido
 * nos fios do sensor — por exemplo, por fios de motor de passo passando em
 * paralelo a eles — pode fazer o termopar reportar erros espúrios. Este valor
 * é a quantidade de erros que podem ocorrer em sequência antes do erro ser
 * reportado. Isso permite ignorar condições de erro intermitentes e ainda
 * detectar uma falha real, que resultaria num fluxo contínuo de erros.
 *
 * Zere este valor para falhar no primeiro erro que ocorrer.
 */
#define THERMOCOUPLE_MAX_ERRORS 15

//
// Parâmetros do termistor personalizado (tipo 1000)
//
#if TEMP_SENSOR_0 == 1000
  #define HOTEND0_PULLUP_RESISTOR_OHMS    4700 // Resistor de pull-up
  #define HOTEND0_RESISTANCE_25C_OHMS   100000 // Resistência a 25°C
  #define HOTEND0_BETA                    3950 // Valor beta
  #define HOTEND0_SH_C_COEFF                 0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_1 == 1000
  #define HOTEND1_PULLUP_RESISTOR_OHMS    4700 // Resistor de pull-up
  #define HOTEND1_RESISTANCE_25C_OHMS   100000 // Resistência a 25°C
  #define HOTEND1_BETA                    3950 // Valor beta
  #define HOTEND1_SH_C_COEFF                 0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_2 == 1000
  #define HOTEND2_PULLUP_RESISTOR_OHMS    4700 // Resistor de pull-up
  #define HOTEND2_RESISTANCE_25C_OHMS   100000 // Resistência a 25°C
  #define HOTEND2_BETA                    3950 // Valor beta
  #define HOTEND2_SH_C_COEFF                 0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_3 == 1000
  #define HOTEND3_PULLUP_RESISTOR_OHMS    4700 // Resistor de pull-up
  #define HOTEND3_RESISTANCE_25C_OHMS   100000 // Resistência a 25°C
  #define HOTEND3_BETA                    3950 // Valor beta
  #define HOTEND3_SH_C_COEFF                 0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_4 == 1000
  #define HOTEND4_PULLUP_RESISTOR_OHMS    4700 // Resistor de pull-up
  #define HOTEND4_RESISTANCE_25C_OHMS   100000 // Resistência a 25°C
  #define HOTEND4_BETA                    3950 // Valor beta
  #define HOTEND4_SH_C_COEFF                 0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_5 == 1000
  #define HOTEND5_PULLUP_RESISTOR_OHMS    4700 // Resistor de pull-up
  #define HOTEND5_RESISTANCE_25C_OHMS   100000 // Resistência a 25°C
  #define HOTEND5_BETA                    3950 // Valor beta
  #define HOTEND5_SH_C_COEFF                 0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_6 == 1000
  #define HOTEND6_PULLUP_RESISTOR_OHMS    4700 // Resistor de pull-up
  #define HOTEND6_RESISTANCE_25C_OHMS   100000 // Resistência a 25°C
  #define HOTEND6_BETA                    3950 // Valor beta
  #define HOTEND6_SH_C_COEFF                 0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_7 == 1000
  #define HOTEND7_PULLUP_RESISTOR_OHMS    4700 // Resistor de pull-up
  #define HOTEND7_RESISTANCE_25C_OHMS   100000 // Resistência a 25°C
  #define HOTEND7_BETA                    3950 // Valor beta
  #define HOTEND7_SH_C_COEFF                 0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_BED == 1000
  #define BED_PULLUP_RESISTOR_OHMS        4700 // Resistor de pull-up
  #define BED_RESISTANCE_25C_OHMS       100000 // Resistência a 25°C
  #define BED_BETA                        3950 // Valor beta
  #define BED_SH_C_COEFF                     0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_CHAMBER == 1000
  #define CHAMBER_PULLUP_RESISTOR_OHMS    4700 // Resistor de pull-up
  #define CHAMBER_RESISTANCE_25C_OHMS   100000 // Resistência a 25°C
  #define CHAMBER_BETA                    3950 // Valor beta
  #define CHAMBER_SH_C_COEFF                 0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_COOLER == 1000
  #define COOLER_PULLUP_RESISTOR_OHMS     4700 // Resistor de pull-up
  #define COOLER_RESISTANCE_25C_OHMS    100000 // Resistência a 25°C
  #define COOLER_BETA                     3950 // Valor beta
  #define COOLER_SH_C_COEFF                  0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_PROBE == 1000
  #define PROBE_PULLUP_RESISTOR_OHMS      4700 // Resistor de pull-up
  #define PROBE_RESISTANCE_25C_OHMS     100000 // Resistência a 25°C
  #define PROBE_BETA                      3950 // Valor beta
  #define PROBE_SH_C_COEFF                   0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_BOARD == 1000
  #define BOARD_PULLUP_RESISTOR_OHMS      4700 // Resistor de pull-up
  #define BOARD_RESISTANCE_25C_OHMS     100000 // Resistência a 25°C
  #define BOARD_BETA                      3950 // Valor beta
  #define BOARD_SH_C_COEFF                   0 // Coeficiente C de Steinhart-Hart
#endif

#if TEMP_SENSOR_REDUNDANT == 1000
  #define REDUNDANT_PULLUP_RESISTOR_OHMS  4700 // Resistor de pull-up
  #define REDUNDANT_RESISTANCE_25C_OHMS 100000 // Resistência a 25°C
  #define REDUNDANT_BETA                  3950 // Valor beta
  #define REDUNDANT_SH_C_COEFF               0 // Coeficiente C de Steinhart-Hart
#endif

/**
 * Opções de termopar — para MAX6675 (-2), MAX31855 (-3) e MAX31865 (-5).
 */
//#define TEMP_SENSOR_FORCE_HW_SPI                // Ignora os pinos SCK/MOSI/MISO; usa o CS e o barramento SPI padrão.
//#define MAX31865_SENSOR_WIRES_0   2             // (2-4) Número de fios do sensor ligado a uma placa MAX31865.
//#define MAX31865_SENSOR_WIRES_1   2
//#define MAX31865_SENSOR_WIRES_2   2
//#define MAX31865_SENSOR_WIRES_BED 2

//#define MAX31865_50HZ_FILTER                    // Usa filtro de 50Hz em vez dos 60Hz padrão.
//#define MAX31865_USE_READ_ERROR_DETECTION       // Trata picos de valor (20°C de variação em menos de 1s) como erro de leitura.

//#define MAX31865_USE_AUTO_MODE                  // Lê mais rápido e com mais frequência que o modo 1-shot; tensão de polarização sempre ligada; leve efeito na temperatura do RTD.
//#define MAX31865_MIN_SAMPLING_TIME_MSEC     100 // (ms) Modo 1-shot: intervalo mínimo de leitura. Reduz o efeito da tensão de polarização deixando o sensor sem energia por mais tempo.
//#define MAX31865_IGNORE_INITIAL_FAULTY_READS 10 // Ignora algumas falhas de leitura (mantendo a temperatura lida), contornando um possível problema (#23439).

//#define MAX31865_WIRE_OHMS_0              0.95f // Em ligação a 2 fios, informe a resistência dos fios para leituras mais precisas.
//#define MAX31865_WIRE_OHMS_1              0.0f
//#define MAX31865_WIRE_OHMS_2              0.0f
//#define MAX31865_WIRE_OHMS_BED            0.0f

/**
 * Kit de atualização da mesa aquecida 24V da Hephestos 2.
 * https://www.en3dstudios.com/product/bq-hephestos-2-heated-bed-kit/
 */
//#define HEPHESTOS2_HEATED_BED_KIT
#if ENABLED(HEPHESTOS2_HEATED_BED_KIT)
  #define HEATER_BED_INVERTING true
