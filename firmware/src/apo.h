#pragma once
/* Auto Power Off (APO) del RT-1000Pro.
 *
 * El firmware original V1.16 trae las entradas de menu "APO" y
 * "APO Timer" (cadenas en 0x17670 y 0x17684) y el mensaje
 * " Power Off! " (0x2076f), pero el firmware open nunca las
 * porto: no habia ajuste, ni temporizador, ni camino de apagado,
 * asi que el APO simplemente no existia. Este modulo es la
 * logica pura (sin HAL): el menu y el apagado viven en app.c.
 *
 * Valores del temporizador: el V1.16 solo da los nombres, no
 * los minutos, asi que las opciones son eleccion nuestra
 * (OFF / 15 / 30 / 60 / 120 min), no un dato decodificado.
 * Por defecto OFF: el radio no debe apagarse solo la primera
 * vez que se usa el firmware open.
 *
 * Actividad que reinicia el temporizador: cualquier tecla, una
 * senal recibida por encima del squelch, o un beacon APRS.
 * El NMEA del GPS NO cuenta: llega siempre y el APO no
 * dispararia nunca. El scan tampoco reinicia: un scan sin
 * senal es inactividad y el APO debe poder apagar el radio.
 */
#include <stdint.h>
#include <stdbool.h>

/* Aviso previo al apagado: 10 s en pantalla "Power Off!",
 * cualquier tecla lo cancela. */
#define APO_WARN_MS 10000u
#define APO_NOPTS 5

/* Minutos del temporizador segun el indice guardado (0=OFF).
 * Indice fuera de rango -> OFF. */
uint32_t rt1000_apo_minutes(uint8_t idx);
uint32_t rt1000_apo_timeout_ms(uint8_t idx);

/* Milisegundos que quedan hasta el apagado (0 si OFF o vencido).
 * Resta en uint32_t: segura ante el wrap de hal_millis(). */
uint32_t rt1000_apo_remaining_ms(uint32_t last_activity_ms, uint32_t now_ms,
                                 uint8_t idx);
bool rt1000_apo_warn_due(uint32_t last_activity_ms, uint32_t now_ms,
                         uint8_t idx);
bool rt1000_apo_due(uint32_t last_activity_ms, uint32_t now_ms, uint8_t idx);

/* Senal recibida: mismo umbral que usa el scan para parar
 * (app.c): -120 dBm + 4 dB por nivel de squelch. */
bool rt1000_apo_signal_active(int16_t rssi_dbm, uint8_t sql);

/* Sanea un indice leido de flash vieja (0xFF u otro): fuera de
 * rango -> OFF. */
uint8_t rt1000_apo_idx_sanitize(uint8_t idx);
