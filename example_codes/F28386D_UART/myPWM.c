//=================================================================================================
/// @file       myPWM.c
///
/// @brief      Datei enth�lt Variablen und Funktionen um die Funktion der ePWM-Module zu
///							demonstrieren. Das ePWM8-Modul wird so initialisiert, dass alle 5 ms ein
///							Interrupt ausgel�st wird und so als Zeitgeber f�r periodisch zu erledigende
///							Aufgaben genutzt werden kann.
///
/// @version    V1.2
///
/// @date       13.02.2023
///
/// @author     Daniel Urbaneck
//=================================================================================================
//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------
#include "myPWM.h"


//-------------------------------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// Global functions
//-------------------------------------------------------------------------------------------------
//=== Function: PwmInitPwm8 =======================================================================
///
/// @brief  Funktion initialisiert das ePWM8-Modul um alle 5 ms einen Interrupt auszul�sen
///
/// @param  void
///
/// @return void
///
//=================================================================================================
void PwmInitPwm8(void)
{
    // Register-Schreibschutz aufheben
		EALLOW;
    // Synchronisierungstakt w�hrend der Konfiguration ausschalten
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 0;

    // Takt f�r das PWM8-Modul einschalten und 5 Takte warten, bis der Takt zum
    // Modul durchgestellt ist (siehe S. 169 Reference Manual TMS320F2838x,
    // SPRUII0D, Rev. D, July 2022)
    CpuSysRegs.PCLKCR2.bit.EPWM8 = 1;
    __asm(" RPT #4 || NOP");
		// Z�hlrichtung des Timers: hoch
    EPwm8Regs.TBCTL.bit.CTRMODE = PWM_TB_COUNT_UP;
		// Taktteiler: 40
    // TBCLK = EPWMCLK / (HSPCLKDIV * CLKDIV)
    // EPWMCLK = SYSCLK / 2 = 100 MHz
    // (siehe "DeviceInit()" und S. 165 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    EPwm8Regs.TBCTL.bit.CLKDIV    = PWM_CLK_DIV_4;
		EPwm8Regs.TBCTL.bit.HSPCLKDIV = PWM_HSPCLKDIV_10;
		// TBCTR nicht mit Wert aus dem Phasenregister laden
    EPwm8Regs.TBCTL.bit.PHSEN = PWM_TB_PHSEN_DISABLE;
    // Daten, welche in das TBPRD Register (Periodendauer) geschrieben
    // werden, direkt laden (ohne Umweg �ber das Shadow-Register)
    EPwm8Regs.TBCTL.bit.PRDLD = PWM_TB_IMMEDIATE;
    // Periodendauer auf 5 ms setzen:
    // Periodendauer = (HSPCLKDIV * CLKDIV * (TBPRD+1)) / EPWMCLK
    // EPWMCLK = SYSCLK / 2 = 100 MHz
    // (siehe "DeviceInit()" und S. 165 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    EPwm8Regs.TBPRD = 12499;
		// Z�hler auf 0 setzen
		EPwm8Regs.TBCTR = 0;
		// PWM8-Interrupt einschalten
		EPwm8Regs.ETSEL.bit.INTEN = PWM_ET_INT_ENABLE;
    // Interrupt ausl�sen, wenn der Timer den Endwert (TBPRD) erreicht hat
		EPwm8Regs.ETSEL.bit.INTSEL = PWM_ET_CTR_PRD;
    // ISR aufrufen, sobald 1 Interrupt ausgel�st wurde
		EPwm8Regs.ETPS.bit.INTPRD = PWM_ET_1ST;

    // Interrupt-Service-Routinen f�r den ePWM8-Interrupt an die
    // entsprechende Stelle (ePWM8_INT) der PIE-Vector Table speichern
    PieVectTable.EPWM8_INT = &Pwm8ISR;
    // INT3.8-Interrupt freischalten (Zeile 3, Spalte 8 der Tabelle 3-2)
    // (siehe S. 150 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    PieCtrlRegs.PIEIER3.bit.INTx8 = 1;
    // CPU-Interrupt 3 einschalten (Zeile 3 der Tabelle)
    IER |= M_INT3;

    // Synchronisierungstakt wieder einschalten
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 1;
		// Register-Schreibschutz setzen
		EDIS;
}


//=== Function: Epwm8ISR ==========================================================================
///
/// @brief  ISR wird alle 5 ms aufgerufen. Wird als Zeitgeber f�r den Aufruf der Funktion
///					"UartGetStatusRxA()" verwendet. Diese Funktion �berwacht den Empfang von UART-Daten
///
/// @param  void
///
/// @return void
///
//=================================================================================================
__interrupt void Pwm8ISR(void)
{
		// Bei jedem Eintritt in eine Interrupt-Service-Routine (ISR) wird automatisch
		// das EALLOW-Bit gel�scht, unabh�ngig davon, ob es zuvor gesetzt war (siehe
		// S. 148 Punkt 9, Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022).
		// Nach Abschluss der ISR wird das EALLOW-Bit automatisch wieder gesetzt, falls
		// es vor dem Auftritt des Interrupts gesetzt war. Falls in der ISR nur auf
		// Register ohne Schreibschutz zugegriffen wird, kann auf den folgenden Befehl
		// verzichtet werden (siehe Spalte "Write Protection" in der Register�bersicht)
		//EALLOW;

		// Flag f�r den Aufruf der Funktion "UartGetStatusRx()" setzen
		uartFlagCheckRxA = true;
		// Timeout-Z�hler herunterz�hlen
		if (uartRxTimeoutA > 0)
		{
				uartRxTimeoutA--;
		}

    // Interrupt-Flag im ePWM8-Modul l�schen
		EPwm8Regs.ETCLR.bit.INT = 1;
    // Interrupt-Flag der Gruppe 3 l�schen (da geh�rt der ePMW8-Interrupt zu)
		PieCtrlRegs.PIEACK.bit.ACK3 = 1;
}



