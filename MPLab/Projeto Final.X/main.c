/*
 * File:   main.c
 * Author: Leticia Veran e Valentina Ragnini
 *
 * Descrição: Sistema Embarcado de Irrigação Inteligente e Controle de pH.
 * O sistema monitora a umidade do solo e o pH da água no reservatório,
 * automatizando o processo de abastecimento (priorizando água pluvial/rio),
 * ajuste químico (pH ideal 5.5-6.5) e irrigação até a umidade alvo (60%).
 * Inclui proteção contra sobrecarga de energia (BOREN) e travamento
 * de software (Watchdog Timer - WDT) para operação contínua e segura.
 *
 * Microcontrolador: PIC16F877A
 * Clock: 4MHz XT
 */

#include <xc.h>
#include <pic16f877a.h>
#include <stdio.h>

#define _XTAL_FREQ 4000000 	 // Define a frequência de operação do microcontrolador (4MHz)

// --- Configurações do Fusível (Configuration Bits) ---
#pragma config FOSC = XT 	 // Oscilador: Cristal externo (XT)
#pragma config WDTE = ON 	 // Watchdog Timer: Ativado (ON) para prevenção de travamentos
#pragma config PWRTE = ON 	 // Power-up Timer: Ativado
#pragma config BOREN = ON 	 // Brown-out Reset: Reinicia em caso de queda de tensão (proteção)
#pragma config LVP = OFF 	 // Low Voltage Programming: Desativado
#pragma config CPD = OFF 	 // Data Code Protection: Desativado
#pragma config WRT = OFF 	 // Write Protection: Desativado
#pragma config CP = OFF 	 // Code Protection: Desativado

// --- Mapeamento do LCD (PORTD) ---
#define RS RD2
#define EN RD3
#define D4 RD4
#define D5 RD5
#define D6 RD6
#define D7 RD7

#include "lcd.h" // Incluindo a biblioteca do Display LCD

// --- Mapeamento do Hardware (PORTC e PORTB) ---
// Saídas (PORTC - Atuadores)
#define acido PORTCbits.RC0 	 // Bomba ou Válvula de dosagem de Ácido
#define base PORTCbits.RC1 	 // Bomba ou Válvula de dosagem de Base
#define bombario PORTCbits.RC2 	 // Bomba principal para captação de água do rio/poço
#define escoamento PORTCbits.RC3 // Válvula de escoamento (dreno) para Nível Alto
#define aspersor PORTCbits.RC4 	 // Válvula de Irrigação/Aspersor
#define misturador PORTCbits.RC5 // Atuador do misturador de pH no reservatório

// Entradas Digitais (PORTB)
#define nivelA2 PORTBbits.RB4 	 // Sensor de Nível Baixo/Crítico (Digital)
#define nivel PORTBbits.RB6 	 // Sensor de Nível Adequado para Operação (Digital)
#define nivelA PORTBbits.RB1 	 // Sensor de Nível Alto/Transbordo (Digital - Interrupção Externa)
#define irrigacao PORTBbits.RB2 	 // Botão de Irrigação Forçada (Interrupção Externa)

// Entradas Analógicas (AN0 e AN1)
// Umidade (AN0), pH (AN1) - Definidas dentro das funções ler_umidade() e ler_ph()

// --- Variáveis Globais de Controle ---
// 'volatile' é usado para variáveis que podem ser alteradas por rotinas de interrupção (ISR)
volatile int flagirrig = 0; 	 // Flag: 1 se a irrigação foi solicitada (Automático ou Forçado)
volatile int flagtimer = 0; 	 // Flag: 1 se o tempo definido pelo Timer 1 expirou
volatile int conta = 0; 	 // Contador de ticks do Timer 1 (passos de 0.5s)
volatile int tempo = 0; 	 // Valor alvo do contador (tempo total a ser esperado)

// --- Protótipos de Funções ---
unsigned int ler_umidade();
unsigned int ler_ph();
void displayI();
void displayPH();
void Inicializacoes();
void EmEspera();
void esperar_timer(int slots_de_0_5s);

// --- Rotina de Serviço da Interrupção (ISR) ---
// Gerencia eventos assíncronos (Botões e Timer)
void __interrupt() TrataInt(void)
{
    // 1. Interrupção Externa (INTF - Interrupção no pino RB0)
    if (INTCONbits.INTF)
    {
        INTCONbits.INTF = 0; // Limpa a flag da Interrupção Externa
        
         // Irrigação Forçada solicitada
        flagirrig = 1;
        
    }

    // 2. Interrupção do Timer 1 (TMR1IF)
    if (PIR1bits.TMR1IF)
    {
        PIR1bits.TMR1IF = 0; // Limpa a flag do Timer 1
        
        // Recarrega o Timer 1 para a contagem de 0.5 segundos (dependente do clock e prescaler)
        TMR1L = 0xDC;
        TMR1H = 0x0B;
        
        conta++; // Incrementa o contador de tempo (cada conta = 0.5s)
        
        // Verifica se o tempo alvo (tempo) foi atingido
        if (conta >= tempo) {
            flagtimer = 1; // Sinaliza que a espera acabou
            conta = 0;
        }
    }
}

// --- Função de Espera Segura (Timer Controlado e WDT Limpo) ---
// Utiliza o Timer 1 (Interrupção) para uma espera precisa sem bloquear o WDT.
void esperar_timer(int slots_de_0_5s) {
    tempo = slots_de_0_5s; 	 // Define o tempo de espera (em unidades de 0.5s)
    conta = 0;
    flagtimer = 0;
    
    // Configura o valor inicial do Timer 1
    TMR1L = 0xDC;
    TMR1H = 0x0B;
    T1CONbits.TMR1ON = 1; 	 // Liga o Timer 1
    
    // Loop de espera: Executa o CLRWDT até que a flagtimer seja levantada pela ISR
    while(flagtimer == 0) {
        CLRWDT(); // Alimenta o Watchdog Timer (impede o reset por timeout)
    }
    
    T1CONbits.TMR1ON = 0; 	 // Desliga o Timer 1
    flagtimer = 0;
}

void Inicializacoes() {
    // Configuração dos Registradores de Direção (TRIS - 0: Saída, 1: Entrada)
    TRISB = 0b11111111; // PORTB: Todo como Entrada (Sensores, Botão)
    TRISD = 0b00000000; // PORTD: Todo como Saída (LCD)
    TRISC = 0b00000000; // PORTC: Todo como Saída (Atuadores/Bombas)

    // --- Configuração do Watchdog Timer (WDT) ---
    // O WDT será lento para dar tempo às rotinas (e.g., ajuste de pH).
    OPTION_REGbits.nRBPU = 0; 	 // Habilita Pull-ups internos
    OPTION_REGbits.INTEDG = 0; 	 // Interrupção Externa na borda de descida
    
    // Configura o Prescaler para o WDT (1:128)
    // O WDT base do PIC é ~18ms. Com 1:128, o timeout é ~2.3 segundos.
    OPTION_REGbits.PSA = 1; 	 
    OPTION_REGbits.PS0 = 1; 	 
    OPTION_REGbits.PS1 = 1;
    OPTION_REGbits.PS2 = 1;

    // --- Habilitação das Interrupções ---
    INTCONbits.GIE = 1; 	 // Global Interrupt Enable (Habilita todas)
    INTCONbits.INTE = 1; 	 // External Interrupt Enable (RB0, usado para RB1/RB2)
    INTCONbits.PEIE = 1; 	 // Peripheral Interrupt Enable
    PIE1bits.TMR1IE = 1; 	 // Timer 1 Interrupt Enable
    
    // --- Configuração do Timer 1 (Contagem de tempo) ---
    T1CONbits.TMR1CS = 0; 	 // Fonte de Clock Interna (Fosc/4)
    T1CONbits.T1CKPS0 = 1; 	 // Prescaler 1:8
    T1CONbits.T1CKPS1 = 1;
    // Carga para 0.5s (calculada para 4MHz, 1:8)
    TMR1L = 0xDC;
    TMR1H = 0x0B;
    T1CONbits.TMR1ON = 0; 	 // Inicia desligado
    
    // --- Configuração do Conversor Analógico-Digital (ADC) ---
    ADCON1bits.PCFG0 = 0; 	 // Todos os pinos Analógicos (AN0-AN4)
    ADCON1bits.PCFG1 = 0;
    ADCON1bits.PCFG2 = 0;
    ADCON1bits.PCFG3 = 0;
    ADCON0bits.ADCS0 = 0; 	 // Clock de conversão do ADC (Fosc/2)
    ADCON0bits.ADCS1 = 0;
    ADCON1bits.ADFM = 1; 	 // Justificativa à direita (padrão)
    ADRESL = 0x00;
    ADRESH = 0x00;
    ADCON0bits.ADON = 1; 	 // Liga o módulo ADC
    
    Lcd_Init(); 	 // Inicializa o display LCD
    PORTD = 0x00; 	 // Zera saídas digitais
    PORTC = 0x00;
}

// --- Funções de Estado (Exibição de Informações) ---

void EmEspera() {
    char buffer[16];
    
    // Leitura e Exibição da Umidade
    unsigned int umid = ler_umidade();
    sprintf(buffer, "Umidade: %u %%     ", umid);
    Lcd_Set_Cursor(1, 1);
    Lcd_Write_String(buffer);

    // Leitura e Exibição do pH
    unsigned int ph_val = ler_ph();
    // pH é formatado com 2 casas decimais (ex: 700 -> 7.00)
    sprintf(buffer, "pH: %u.%02u             ", ph_val/100, ph_val%100); 
    Lcd_Set_Cursor(2, 1);
    Lcd_Write_String(buffer);

    // Pequena pausa para visualização e para alimentar o WDT
    __delay_ms(200); 
    CLRWDT(); // Impede o reset do WDT durante o estado de espera
}

void displayI() {
    char buffer[16];
    sprintf(buffer, "Irrigacao!!");
    Lcd_Set_Cursor(1, 1);
    Lcd_Write_String(buffer);
    sprintf(buffer, "Umidade: %u %%", ler_umidade());
    Lcd_Set_Cursor(2, 1);
    Lcd_Write_String(buffer);
}

void displayPH() {
    char buffer[16];
    Lcd_Set_Cursor(1, 1);
    Lcd_Write_String("Ajuste de PH   ");
    unsigned int p = ler_ph();
    sprintf(buffer, "PH: %u.%02u ", p/100, p%100);
    Lcd_Set_Cursor(2, 1);
    Lcd_Write_String(buffer);
}

// --- Funções de Leitura do ADC ---

unsigned int ler_umidade() {
    // Seleciona o canal analógico 0 (AN0)
    ADCON0bits.CHS0 = 0; ADCON0bits.CHS1 = 0; ADCON0bits.CHS2 = 0;
    __delay_us(20); 	 // Tempo de aquisição
    ADCON0bits.GO = 1; 	 // Inicia a conversão
    while(ADCON0bits.GO); // Aguarda a conclusão da conversão
    unsigned int leitura = ((ADRESH << 8) + ADRESL);
    // Mapeia o valor de 0-1023 para porcentagem (0-100%)
    return (unsigned int)( ((unsigned long)leitura * 100) / 1023 );
}

unsigned int ler_ph() {
    // Seleciona o canal analógico 1 (AN1)
    ADCON0bits.CHS0 = 1; ADCON0bits.CHS1 = 0; ADCON0bits.CHS2 = 0;
    __delay_us(20);
    ADCON0bits.GO = 1;
    while(ADCON0bits.GO);
    unsigned int leitura = ((ADRESH << 8) + ADRESL);
    // Mapeia o valor de 0-1023 para o valor de pH (multiplicado por 100).
    // Ex: 1023 -> 14.00, 511 -> 7.00. (Simplificação da curva de calibração)
    return (unsigned int)( ((unsigned long)leitura * 1400) / 1023 );
}

// --- Função Principal ---

void main(void) {
    
    Inicializacoes();
    char buffer[16];

    // Constantes de Tempo (em slots de 0.5s) - tempo curto para simulação
    const int T_PADRAO = 2; 	 // Dosagem padrão 
    const int T_EXTRA = 2; 	 // Dosagem extra 
    const int T_MISTURA = 2; 	 // Tempo de mistura 
    const int T_ESTABILIZA = 2; // Tempo de espera para estabilização do pH 

    // --- Loop Principal (Máquina de Estados) ---
    while(1)
    {
        CLRWDT(); // SEGURANÇA: Alimenta o WDT no início de cada ciclo principal

    // --- ESTADO DE PROTEÇÃO (Escoamento): Prioridade alta, ativado pela checagem direta de nivelA (0 = Nível Alto/Ativo). ---
        if (nivelA == 0) {
            Lcd_Clear();
            Lcd_Set_Cursor(1, 1);
            Lcd_Write_String("Nivel Alto!!");
            Lcd_Set_Cursor(2, 1);
            Lcd_Write_String("Escoamento ativo");

            escoamento = 1; 
            
            // Drena até que ambos os sensores de nível alto e baixo (nivelA2, nivelA) desativem
            while (nivelA2 == 0 || nivelA == 0) {
                CLRWDT(); // Segurança: Rotina potencialmente longa
                escoamento = 1;
                __delay_ms(100); 
            }
            escoamento = 0; 	 // Desliga o dreno
            Lcd_Clear(); 	 
        }
        
        // --- ESTADO DE MONITORAMENTO (Modo de Espera) ---
        EmEspera();
        
        // --- ROTINA PRINCIPAL (Gatilho Automático ou Interrupção Manual) ---
        // Verifica se há pedido de irrigação (Automático: Umidade < 40% OU Manual: flagirrig)
        if (ler_umidade() < 40 || flagirrig == 1) {
            
            // Passo A: VERIFICA E ENCHE ÁGUA (ESTADO 2)
            // Lógica: Se o sensor de Nível Adequado (nivel) estiver desligado (0), o tanque está cheio.
            if (nivel != 0) {
                // Tanque não está no nível adequado, iniciar enchimento
                while (nivel != 0) { 
                    CLRWDT(); 
                    Lcd_Clear();
                    Lcd_Set_Cursor(1, 1);
                    Lcd_Write_String("Bomba Ligada!!  ");
                    Lcd_Set_Cursor(2, 1);
                    Lcd_Write_String("Enchendo Nivel    ");
                    
                    bombario = 1; 	 // Liga bomba do rio/poço
                    __delay_ms(200); 
                }
                bombario = 0; // Desliga a bomba ao atingir o Nível Adequado
            }

            // Passo B: AJUSTE DE PH (ESTADO 3)
            // Faixa alvo: 5.50 a 6.50 (representado como 550 e 650)
            if (ler_ph() < 550 || ler_ph() > 650) {
                Lcd_Clear();
                
                // Loop de correção de pH
                while (ler_ph() < 550 || ler_ph() > 650) {
                    CLRWDT(); 
                    displayPH(); 
                    
                    if (ler_ph() > 650) { // Muito Básico, adicionar Ácido
                        acido = 1;
                        // Dosagem adaptativa: maior se muito fora do alvo
                        if (ler_ph() > 750) esperar_timer(T_PADRAO + T_EXTRA); 
                        else esperar_timer(T_PADRAO);
                        acido = 0;
                    }
                    else if (ler_ph() < 550) { // Muito Ácido, adicionar Base
                        base = 1;
                        // Dosagem adaptativa
                        if (ler_ph() < 450) esperar_timer(T_PADRAO + T_EXTRA);
                        else esperar_timer(T_PADRAO);
                        base = 0;
                    }
                    
                    // Mistura e estabiliza
                    misturador = 1;
                    esperar_timer(T_MISTURA);
                    misturador = 0;
                    esperar_timer(T_ESTABILIZA);
                }
            }
            
            // Passo C: IRRIGANDO (ESTADO 4)
            Lcd_Clear();
            unsigned int umidade_atual = ler_umidade();

            if (flagirrig == 1 && umidade_atual >= 60) {
                // Irrigação Forçada (seja a terra seca ou molhada, usa o timer de 10s - 20 slots)
                Lcd_Set_Cursor(1, 1);
                Lcd_Write_String("Irrig. Forcada ");
                Lcd_Set_Cursor(2, 1);
                Lcd_Write_String("Tempo: 10s      ");
                
                aspersor = 1;
                esperar_timer(2); // 20 slots * 0.5s/slot = 10 segundos, coloquei menos para simulação
                aspersor = 0;
                
            } else if (umidade_atual < 60) { 
                // Irrigação Automática (solo seco), irriga até 60%
                while (ler_umidade() < 60) {
                    CLRWDT(); 
                    displayI(); 
                    aspersor = 1;
                }
                aspersor = 0;
            }
            
            flagirrig = 0; // Finaliza o ciclo (manual ou automático)
            Lcd_Clear();
        }
        
        // Se a umidade estiver OK e não houver flags, volta para EmEspera (Monitoramento)
    }
}

