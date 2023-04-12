//=================================================================================================
/// @file       myPWM.c
///
/// @brief      Datei enth�lt Variablen und Funktionen um die Funktion der PWM-Module zu
///							demonstrieren. Die PWM-Module 1 bis 4 werden so initialisiert, dass sie mit
///							100 MHz laufen und ein 20 kHz-Signal mit variablen Tastverh�ltnis (Aufl�sung
///							= 50 kHz) jeweils am Kanal A augeben. Das PWM-Modul 8 erzeugt alle 10 ms einen
///							Interrupt sowie einen ADC-Trigger zum Starten einer Messung.
///
/// @version    V1.2
///
/// @date       09.03.2023
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
// Z�hler zum Blinken der LEDs auf dem Control-Board
uint16_t counterToggleLeds = 0;


//-------------------------------------------------------------------------------------------------
// Global functions
//-------------------------------------------------------------------------------------------------
//=== Function: PwmInitPWM1To4 ====================================================================
///
/// @brief  Funktion initialisiert das ePWM1-, ePWM2-, ePWM3- und ePWM4-Modul um vier
///					LEDs auf dem Docking-Board anzusteuern. Dabei werden die LEDs �ber Ver�nderung
///					des Tastverh�ltnisses gedimmt. Es wird jeweils der PWM-Kanal A verwendet.
///					Das PWM1-Modul ist der Master und Synchronisiert die anderen drei Module.
///
/// @param  void
///
/// @return void
///
//=================================================================================================
void PwmInitPWM1To4(void)
{
    // Register-Schreibschutz aufheben
    EALLOW;
    // Synchronisierungstakt w�hrend der Konfiguration ausschalten
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 0;

    // PWM1-Modul:
    // Takt f�r das PWM1-Modul einschalten und 5 Takte
    // warten, bis der Takt zum Modul durchgestellt ist
    // (siehe S. 169 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    CpuSysRegs.PCLKCR2.bit.EPWM1 = 1;
    __asm(" RPT #4 || NOP");
    // Takt-Teiler des PWM-Moduls setzen
    // TBCLK = EPWMCLK / (HSPCLKDIV * CLKDIV)
    // EPWMCLK = SYSCLK / 2 = 100 MHz
    // (siehe "DeviceInit()" und S. 165 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    EPwm1Regs.TBCTL.bit.CLKDIV    = PWM_CLK_DIV_1;
    EPwm1Regs.TBCTL.bit.HSPCLKDIV = PWM_HSPCLKDIV_1;
    // TBCTR nicht mit Wert aus dem Phasenregister laden
    EPwm1Regs.TBCTL.bit.PHSEN = PWM_TB_PHSEN_DISABLE;
    // Kein Phasenversatz
    EPwm1Regs.TBPHS.bit.TBPHS = 0;
    // Synchronisations-Impuls (SYNCOUT) erzeugen, wenn Timer = 0
    // (siehe S. 2865 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    EPwm1Regs.EPWMSYNCOUTEN.bit.ZEROEN = 1;
    // Kontinuierlicher Synchronisationsbetrieb
    EPwm1Regs.TBCTL2.bit.OSHTSYNCMODE = 0;
    // Betriebsart: hoch z�hlen
    EPwm1Regs.TBCTL.bit.CTRMODE = PWM_TB_COUNT_UP;
    // Wert, welcher in TBPRD geschrieben wird, sofort �bernehmen
    EPwm1Regs.TBCTL.bit.PRDLD = PWM_TB_IMMEDIATE;
    // Periode setzen
    EPwm1Regs.TBPRD = 5000;
    // Wert, welcher in die Compare-Register geschrieben wird, sofort �bernehmen
    EPwm1Regs.CMPCTL.bit.SHDWAMODE = PWM_CC_IMMEDIATE;
    // Tastverh�ltnis auf 0 setzen
    EPwm1Regs.CMPA.bit.CMPA = 0;
    // PWM1A-Pin auf high ziehen, wenn TBCTR (Timer-Z�hlwert) 0 ist
    EPwm1Regs.AQCTLA.bit.ZRO = PWM_AQ_SET;
    // PWM1A-Pin auf low ziehen, wenn TBCTR (Timer-Z�hlwert) den Wert CMPA erreicht
    EPwm1Regs.AQCTLA.bit.CAU = PWM_AQ_CLEAR;
    // Totzeiten (siehe S. 2898 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022):
    // PWM1A-Signal direkt ausgeben (Totzeitmodul umgehen)
    EPwm1Regs.DBCTL.bit.OUT_MODE = PWM_DB_A_BYPASSED;
    // PWM1A-Signal auf OutA legen
    EPwm1Regs.DBCTL.bit.OUTSWAP = PWM_DB_SWAP_AOUT_A_BOUT_A;
    // Z�hler auf 0 setzen
    EPwm1Regs.TBCTR = 0;


    // PWM2-Modul:
    // Takt f�r das PWM2-Modul einschalten und 5 Takte
    // warten, bis der Takt zum Modul durchgestellt ist
    // (siehe S. 169 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    CpuSysRegs.PCLKCR2.bit.EPWM2 = 1;
    __asm(" RPT #4 || NOP");
    // Takt-Teiler des PWM-Moduls setzen
    // TBCLK = EPWMCLK / (HSPCLKDIV * CLKDIV)
    // EPWMCLK = SYSCLK / 2 = 100 MHz
    // (siehe "DeviceInit()" und S. 165 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    EPwm2Regs.TBCTL.bit.CLKDIV    = PWM_CLK_DIV_1;
    EPwm2Regs.TBCTL.bit.HSPCLKDIV = PWM_HSPCLKDIV_1;
    // Synchronisation mit Synchronisationsausgang von ePWM1
    // (siehe S. 2865 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    EPwm2Regs.EPWMSYNCINSEL.bit.SEL = PWM_TB_SYNCIN_EPWM1_SYNCOUT;
    // TBCTR mit Wert aus dem Phasenregister laden,
    // wenn ein Synchronisationsimpuls auftritt
    EPwm2Regs.TBCTL.bit.PHSEN = PWM_TB_PHSEN_ENABLE;
    // Wert, welcher bei einem Synchronisationsimpuls in das Z�hlerregister
    // (TBCTR) geladen wird. Falls der PWM-Takt gleich der Z�hler-Takt ist
    // (CLKDIV = 1, HSPCLKDIV = 1 -> EPWMCLK = TBCLK), wird der Wert aus dem
    // Phasen-Register (TBPHS) 2 Taktzyklen nach dem Synchronisationsimpuls
    // in das Z�hler- Register geladen. Falls der Z�hlertakt kleiner als der
    // PWM-Takt ist (CLKDIV > 1 und/oder HSPCLKDIV > 1 -> EPWMCLK > TBCLK),
    // betr�gt die Verz�gerung 1 Taktzyklus. Im aktuellen Datenblatt (Reference
    // Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022) steht dieses Verhalten
    // nicht beschrieben, es ist jedoch z.B. auf S. 1840 im Reference Manual
    // TMS320F280049C, SPRUI33F, Rev. F, Dec. 2022 dokumentiert. Zudem konnte
    // dieses Verhalten nachgewiesen werden. Im Up-Modus kann diese Verz�gerung
    // durch TBPHS = 2 bzw. TBPHS = 1 kompensiert werden. Im Up-Down-Modus
    // funktioniert das nicht, weil das Slave-PWM-Modul ggf. runter z�hlt w�hrend
    // der Synchronisationsimpuls kommt. In diesem Fall w�rde der Z�hler z.B. von
    // 0 auf 2 bzw. 1 zur�ckspringen und von dort aus wieder runtergez�hlt werden.
    // F�r diesen Betriebsfall kann das Bit TBCTL.bit.PHSDIR verwendet werden.
    // Dieses Bit ist nur wirksam, falls der Up-Down-Modus akiv ist. Es legt die
    // Z�hlrichtung des PWM-Moduls fest, nachdem ein Synchronisationsevent
    // aufgetreten ist und ein Wert aus dem Register TBPHS geladen wurde.
    // Alternative: Zwei Compare-Register verwenden (CMPA und CMPB). beim
    // Hochz�hlen wird dann beispielsweise der Ausgang auf 1 umgeschaltet, wenn
    // TBCTR = CMPA und beim Runterz�hlen auf 0, wenn TBCTR = CMPB. Die CMPA-
    // Register der Slave-PWM-Module werden um den Wert der Verz�gerungszeit
    // (2 oder 1) reduziert und die CMPB-Register um den Wert (2 oder 1) erh�ht
    EPwm2Regs.TBPHS.bit.TBPHS = 0;
    // Betriebsart: hoch z�hlen
    EPwm2Regs.TBCTL.bit.CTRMODE = PWM_TB_COUNT_UP;
    // Wert, welcher in TBPRD geschrieben wird, sofort �bernehmen
    EPwm2Regs.TBCTL.bit.PRDLD = PWM_TB_IMMEDIATE;
    // Periode setzen
    EPwm2Regs.TBPRD = 5000;
    // Wert, welcher in die Compare-Register geschrieben wird, sofort �bernehmen
    EPwm2Regs.CMPCTL.bit.SHDWAMODE = PWM_CC_IMMEDIATE;
    // Tastverh�ltnis auf 0 setzen
    EPwm2Regs.CMPA.bit.CMPA = 0;
    // PWM2A-Pin auf high ziehen, wenn TBCTR (Timer-Z�hlwert) 0 ist
    EPwm2Regs.AQCTLA.bit.ZRO = PWM_AQ_SET;
    // PWM2A-Pin auf low ziehen, wenn TBCTR (Timer-Z�hlwert) den Wert CMPA erreicht
    EPwm2Regs.AQCTLA.bit.CAU = PWM_AQ_CLEAR;
    // Totzeiten (siehe S. 2898 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022):
    // PWM1A-Signal direkt ausgeben (Totzeitmodul umgehen)
    EPwm2Regs.DBCTL.bit.OUT_MODE = PWM_DB_A_BYPASSED;
    // PWM2A-Signal auf OutA legen
    EPwm2Regs.DBCTL.bit.OUTSWAP = PWM_DB_SWAP_AOUT_A_BOUT_A;
    // Z�hler auf 0 setzen
    EPwm2Regs.TBCTR = 0;


    // PWM3-Modul:
    // Takt f�r das PWM3-Modul einschalten und 5 Takte
    // warten, bis der Takt zum Modul durchgestellt ist
    // (siehe S. 169 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    CpuSysRegs.PCLKCR2.bit.EPWM3 = 1;
    __asm(" RPT #4 || NOP");
    // Takt-Teiler des PWM-Moduls setzen
    // TBCLK = EPWMCLK / (HSPCLKDIV * CLKDIV)
    // EPWMCLK = SYSCLK / 2 = 100 MHz
    // (siehe "DeviceInit()" und S. 165 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    EPwm3Regs.TBCTL.bit.CLKDIV    = PWM_CLK_DIV_1;
    EPwm3Regs.TBCTL.bit.HSPCLKDIV = PWM_HSPCLKDIV_1;
    // Synchronisation mit Synchronisationsausgang von ePWM1
    // (siehe S. 2865 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    EPwm3Regs.EPWMSYNCINSEL.bit.SEL = PWM_TB_SYNCIN_EPWM1_SYNCOUT;
    // TBCTR mit Wert aus dem Phasenregister laden,
    // wenn ein Synchronisationsimpuls auftritt
    EPwm3Regs.TBCTL.bit.PHSEN = PWM_TB_PHSEN_ENABLE;
    // Wert, welcher bei einem Synchronisationsimpuls in das Z�hlerregister
    // (TBCTR) geladen wird. Falls der PWM-Takt gleich der Z�hler-Takt ist
    // (CLKDIV = 1, HSPCLKDIV = 1 -> EPWMCLK = TBCLK), wird der Wert aus dem
    // Phasen-Register (TBPHS) 2 Taktzyklen nach dem Synchronisationsimpuls
    // in das Z�hler- Register geladen. Falls der Z�hlertakt kleiner als der
    // PWM-Takt ist (CLKDIV > 1 und/oder HSPCLKDIV > 1 -> EPWMCLK > TBCLK),
    // betr�gt die Verz�gerung 1 Taktzyklus. Im aktuellen Datenblatt (Reference
    // Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022) steht dieses Verhalten
    // nicht beschrieben, es ist jedoch z.B. auf S. 1840 im Reference Manual
    // TMS320F280049C, SPRUI33F, Rev. F, Dec. 2022 dokumentiert. Zudem konnte
    // dieses Verhalten nachgewiesen werden. Im Up-Modus kann diese Verz�gerung
    // durch TBPHS = 2 bzw. TBPHS = 1 kompensiert werden. Im Up-Down-Modus
    // funktioniert das nicht, weil das Slave-PWM-Modul ggf. runter z�hlt w�hrend
    // der Synchronisationsimpuls kommt. In diesem Fall w�rde der Z�hler z.B. von
    // 0 auf 2 bzw. 1 zur�ckspringen und von dort aus wieder runtergez�hlt werden.
    // F�r diesen Betriebsfall kann das Bit TBCTL.bit.PHSDIR verwendet werden.
    // Dieses Bit ist nur wirksam, falls der Up-Down-Modus akiv ist. Es legt die
    // Z�hlrichtung des PWM-Moduls fest, nachdem ein Synchronisationsevent
    // aufgetreten ist und ein Wert aus dem Register TBPHS geladen wurde.
    // Alternative: Zwei Compare-Register verwenden (CMPA und CMPB). beim
    // Hochz�hlen wird dann beispielsweise der Ausgang auf 1 umgeschaltet, wenn
    // TBCTR = CMPA und beim Runterz�hlen auf 0, wenn TBCTR = CMPB. Die CMPA-
    // Register der Slave-PWM-Module werden um den Wert der Verz�gerungszeit
    // (2 oder 1) reduziert und die CMPB-Register um den Wert (2 oder 1) erh�ht
    EPwm3Regs.TBPHS.bit.TBPHS = 0;
    // Wert, welcher in TBPRD geschrieben wird, sofort �bernehmen
    EPwm3Regs.TBCTL.bit.PRDLD = PWM_TB_IMMEDIATE;
    // Betriebsart: hoch z�hlen
    EPwm3Regs.TBCTL.bit.CTRMODE = PWM_TB_COUNT_UP;
    // Periode setzen
    EPwm3Regs.TBPRD = 5000;
    // Wert, welcher in die Compare-Register geschrieben wird, sofort �bernehmen
    EPwm3Regs.CMPCTL.bit.SHDWAMODE = PWM_CC_IMMEDIATE;
    // Tastverh�ltnis auf 0 setzen
    EPwm3Regs.CMPA.bit.CMPA = 0;
    // PWM3A-Pin auf high ziehen, wenn TBCTR (Timer-Z�hlwert) 0 ist
    EPwm3Regs.AQCTLA.bit.ZRO = PWM_AQ_SET;
    // PWM3A-Pin auf low ziehen, wenn TBCTR (Timer-Z�hlwert) den Wert CMPA erreicht
    EPwm3Regs.AQCTLA.bit.CAU = PWM_AQ_CLEAR;
    // Totzeiten (siehe S. 2898 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022):
    // PWM3A-Signal direkt ausgeben (Totzeitmodul umgehen)
    EPwm3Regs.DBCTL.bit.OUT_MODE = PWM_DB_A_BYPASSED;
    // PWM3A-Signal auf OutA legen
    EPwm3Regs.DBCTL.bit.OUTSWAP = PWM_DB_SWAP_AOUT_A_BOUT_A;
    // Z�hler auf 0 setzen
    EPwm3Regs.TBCTR = 0;


    // PWM4-Modul:
    // Takt f�r das PWM4-Modul einschalten und 5 Takte
    // warten, bis der Takt zum Modul durchgestellt ist
    // (siehe S. 169 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    CpuSysRegs.PCLKCR2.bit.EPWM4 = 1;
    __asm(" RPT #4 || NOP");
    // Takt-Teiler des PWM-Moduls setzen
    // TBCLK = EPWMCLK / (HSPCLKDIV * CLKDIV)
    // EPWMCLK = SYSCLK / 2 = 100 MHz
    // (siehe "DeviceInit()" und S. 165 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    EPwm4Regs.TBCTL.bit.CLKDIV    = PWM_CLK_DIV_1;
    EPwm4Regs.TBCTL.bit.HSPCLKDIV = PWM_HSPCLKDIV_1;
    // Synchronisation mit Synchronisationsausgang von ePWM1
    // (siehe S. 2865 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    EPwm4Regs.EPWMSYNCINSEL.bit.SEL = PWM_TB_SYNCIN_EPWM1_SYNCOUT;
    // TBCTR mit Wert aus dem Phasenregister laden,
    // wenn ein Synchronisationsimpuls auftritt
    EPwm4Regs.TBCTL.bit.PHSEN = PWM_TB_PHSEN_ENABLE;
    // Wert, welcher bei einem Synchronisationsimpuls in das Z�hlerregister
    // (TBCTR) geladen wird. Falls der PWM-Takt gleich der Z�hler-Takt ist
    // (CLKDIV = 1, HSPCLKDIV = 1 -> EPWMCLK = TBCLK), wird der Wert aus dem
    // Phasen-Register (TBPHS) 2 Taktzyklen nach dem Synchronisationsimpuls
    // in das Z�hler- Register geladen. Falls der Z�hlertakt kleiner als der
    // PWM-Takt ist (CLKDIV > 1 und/oder HSPCLKDIV > 1 -> EPWMCLK > TBCLK),
    // betr�gt die Verz�gerung 1 Taktzyklus. Im aktuellen Datenblatt (Reference
    // Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022) steht dieses Verhalten
    // nicht beschrieben, es ist jedoch z.B. auf S. 1840 im Reference Manual
    // TMS320F280049C, SPRUI33F, Rev. F, Dec. 2022 dokumentiert. Zudem konnte
    // dieses Verhalten nachgewiesen werden. Im Up-Modus kann diese Verz�gerung
    // durch TBPHS = 2 bzw. TBPHS = 1 kompensiert werden. Im Up-Down-Modus
    // funktioniert das nicht, weil das Slave-PWM-Modul ggf. runter z�hlt w�hrend
    // der Synchronisationsimpuls kommt. In diesem Fall w�rde der Z�hler z.B. von
    // 0 auf 2 bzw. 1 zur�ckspringen und von dort aus wieder runtergez�hlt werden.
    // F�r diesen Betriebsfall kann das Bit TBCTL.bit.PHSDIR verwendet werden.
    // Dieses Bit ist nur wirksam, falls der Up-Down-Modus akiv ist. Es legt die
    // Z�hlrichtung des PWM-Moduls fest, nachdem ein Synchronisationsevent
    // aufgetreten ist und ein Wert aus dem Register TBPHS geladen wurde.
    // Alternative: Zwei Compare-Register verwenden (CMPA und CMPB). beim
    // Hochz�hlen wird dann beispielsweise der Ausgang auf 1 umgeschaltet, wenn
    // TBCTR = CMPA und beim Runterz�hlen auf 0, wenn TBCTR = CMPB. Die CMPA-
    // Register der Slave-PWM-Module werden um den Wert der Verz�gerungszeit
    // (2 oder 1) reduziert und die CMPB-Register um den Wert (2 oder 1) erh�ht
    EPwm4Regs.TBPHS.bit.TBPHS = 0;
    // Wert, welcher in TBPRD geschrieben wird, sofort �bernehmen
    EPwm4Regs.TBCTL.bit.PRDLD = PWM_TB_IMMEDIATE;
    // Betriebsart: hoch z�hlen
    EPwm4Regs.TBCTL.bit.CTRMODE = PWM_TB_COUNT_UP;
    // Periode setzen
    EPwm4Regs.TBPRD = 5000;
    // Wert, welcher in die Compare-Register geschrieben wird, sofort �bernehmen
    EPwm4Regs.CMPCTL.bit.SHDWAMODE = PWM_CC_IMMEDIATE;
    // Tastverh�ltnis auf 0 setzen
    EPwm4Regs.CMPA.bit.CMPA = 0;
    // PWM4A-Pin auf high ziehen, wenn TBCTR (Timer-Z�hlwert) 0 ist
    EPwm4Regs.AQCTLA.bit.ZRO = PWM_AQ_SET;
    // PWM4A-Pin auf low ziehen, wenn TBCTR (Timer-Z�hlwert) den Wert CMPA erreicht
    EPwm4Regs.AQCTLA.bit.CAU = PWM_AQ_CLEAR;
    // Totzeiten (siehe S. 2898 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022):
    // PWM4A-Signal direkt ausgeben (Totzeitmodul umgehen)
    EPwm4Regs.DBCTL.bit.OUT_MODE = PWM_DB_A_BYPASSED;
    // PWM4A-Signal auf OutA legen
    EPwm4Regs.DBCTL.bit.OUTSWAP = PWM_DB_SWAP_AOUT_A_BOUT_A;
    // Z�hler auf 0 setzen
    EPwm4Regs.TBCTR = 0;


    // Synchronisierungstakt einschalten
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC = 1;

		// Register-Schreibschutz setzen
		EDIS;
}


//=== Function: PwmInitPwm8 =======================================================================
///
/// @brief  Funktion initialisiert das ePWM8-Modul um alle 10 ms einen
///         Interrupt auszul�sen und ADC-Messungen zu triggern
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

    // Takt f�r das PWM8-Modul einschalten und 5 Takte
    // warten, bis der Takt zum Modul durchgestellt ist
    // (siehe S. 169 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    CpuSysRegs.PCLKCR2.bit.EPWM8 = 1;
    __asm(" RPT #4 || NOP");
		// Z�hlrichtung des Timers: hoch
    EPwm8Regs.TBCTL.bit.CTRMODE = PWM_TB_COUNT_UP;
		// Taktteiler: 128
    // TBCLK = EPWMCLK / (HSPCLKDIV * CLKDIV)
    // EPWMCLK = SYSCLK / 2 = 100 MHz
    // (siehe "DeviceInit()" und S. 165 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    EPwm8Regs.TBCTL.bit.CLKDIV    = PWM_CLK_DIV_128;
		EPwm8Regs.TBCTL.bit.HSPCLKDIV = PWM_HSPCLKDIV_1;
		// TBCTR nicht mit Wert aus dem Phasenregister laden
    EPwm8Regs.TBCTL.bit.PHSEN = PWM_TB_PHSEN_DISABLE;
    // Daten, welche in das TBPRD Register (Periodendauer) geschrieben
    //  werden, direkt laden (ohne Umweg �ber das Shadow-Register)
    EPwm8Regs.TBCTL.bit.PRDLD = PWM_TB_IMMEDIATE;
    // Periodendauer auf 10 ms setzen:
    // Periodendauer = (HSPCLKDIV * CLKDIV * (TBPRD+1)) / EPWMCLK
    // EPWMCLK = SYSCLK / 2 = 100 MHz
    // (siehe "DeviceInit()" und S. 165 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    EPwm8Regs.TBPRD = 7811;
		// Z�hler auf 0 setzen
		EPwm8Regs.TBCTR = 0;
		// Konfiguration des PWM-Moduls zum Triggern einer ADC-Messung:
		// SOCA-Trigger einschalten
    // 0: SOCA-Trigger ausgeschaltet
    // 1: SOCA-Trigger eingeschaltet
		EPwm8Regs.ETSEL.bit.SOCAEN = PWM_ET_SOC_ENABLE;
		// SOCA-Trigger erzeugen, wenn der PWM-Z�hler (TBCTR) den Wert 0 erreicht
		// 0: Trigger ausgeschaltet
    // 1: Trigger wenn TBCTR = 0
    // 2: Trigger wenn TBCTR = TBPRD
    // 3: Trigger wenn TBCTR = TBPRD oder TBCTR = 0
    // 4: Trigger wenn TBCTR = CMPA (SOCASELCMP = 0) bzw. TBCTR = CMPC (SOCASELCMP = 1) beim Hochz�hlen
    // 5: Trigger wenn TBCTR = CMPA (SOCASELCMP = 0) bzw. TBCTR = CMPC (SOCASELCMP = 1) beim Runterz�hlen
    // 6: Trigger wenn TBCTR = CMPB (SOCASELCMP = 0) bzw. TBCTR = CMPD (SOCASELCMP = 1) beim Hochz�hlen
    // 7: Trigger wenn TBCTR = CMPB (SOCASELCMP = 0) bzw. TBCTR = CMPD (SOCASELCMP = 1) beim Runterz�hlen
		EPwm8Regs.ETSEL.bit.SOCASEL = PWM_ET_CTR_ZERO;
		// SOCA-Trigger bei jedem Event (siehe ETSEL.SOCASEL) erzeugen
		// 0: Event-Trigger ausgeschaltet
		// 1: SOCA-Trigger beim 1. Auftreten des Events ausl�sen
		// 2: SOCA-Trigger beim 2. Auftreten des Events ausl�sen
		// n: SOCA-Trigger beim n. Auftreten des Events ausl�sen
		EPwm8Regs.ETPS.bit.SOCAPRD = PWM_ET_1ST;
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


//=== Function: Pwm8ISR ===========================================================================
///
/// @brief  ISR wird alle 100 ms aufgerufen. Wird als ADC-Trigger und Zeitgeber verwendet
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
		// Z�hler zum Blinken der LEDs auf dem Control-Board hochz�hlen
		if (counterToggleLeds < PWM_FINAL_COUNTER_LEDS)
		{
				counterToggleLeds++;
		}
		else
		{
				counterToggleLeds = 1;
		}

    // Interrupt-Flag im ePWM8-Modul l�schen
		EPwm8Regs.ETCLR.bit.INT = 1;
    // Interrupt-Flag der Gruppe 3 l�schen (da geh�rt der ePMW8-Interrupt zu)
		PieCtrlRegs.PIEACK.bit.ACK3 = 1;
}



