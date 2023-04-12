//=================================================================================================
/// @file       myI2C.c
///
/// @brief      Datei enth�lt Variablen und Funktionen um die I2C-Schnittstelle (Modul A) eines
///							TMS320F2838x als Master zu benutzen. Die Kommunikation ist Interrupt-basiert.
///						  Der Status der Kommunikation (Kommunikation aktiv oder nicht aktiv) kann �ber
///             die Funktion "I2cGetStatus()" abgefragt werden. Es wird das I2C-A Modul verwendet.
///							Das Modul B kann analog zu den hier gezeigten Funktionen verwendet werden.
///
///							�nderung in Version 2.0: Verwendung der Hardware-FIFOs
///
/// @version    V1.4
///
/// @date       27.03.2023
///
/// @author     Daniel Urbaneck
//=================================================================================================
//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------
#include "myI2C.h"


//-------------------------------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------------------------------
// Software-Puffer f�r die I2C-Kommunikation
uint16_t i2cBufferWriteA[I2C_SIZE_HARDWARE_FIFO];
uint16_t i2cBufferReadA[I2C_SIZE_HARDWARE_FIFO];
// �bernimmt die Anzahl der zu lesenden Bytes beim Aufruf der Funktion "I2cWriteReadA()"
// und �bergibt sie dem Register I2CCNT nach einer wiederholten START-Bedingung um die
// gew�nschte Anzahl von Bytes vom Slave zu lesen
uint16_t i2cBytesToReadAfterRSA;
// Flag speichert den aktuellen Zustand der I2C-Kommunikation
uint16_t i2cStatusFlagA;


//-------------------------------------------------------------------------------------------------
// Global functions
//-------------------------------------------------------------------------------------------------
//=== Function: I2cInitA ==========================================================================
///
/// @brief  Funktion initialisiert GPIO 0 (SDA) und GPIO 1 (SCL) als I�C-Pins, das I�C-Modul
///         als Master mit 100 kHz oder 400 kHz Clock, die Adressierung von Slaves auf 7 Bit
///					und die Datengr��e auf 8 Bit. Der I2C-Interrupt wird eingeschaltet und die ISR auf
///					die PIE-Vector-Tabelle gesetzt.
///
/// @param  uint32_t clock
///
/// @return void
///
//=================================================================================================
void I2cInitA(uint32_t clock)
{
    // Register-Schreibschutz aufheben
    EALLOW;

    // GPIO-Sperre f�r GPIO 0 aufheben
    GpioCtrlRegs.GPALOCK.bit.GPIO0 = 0;
    // GPIO 0 auf I2C-Funktion setzen (SDA)
    // Die Zahl in der obersten Zeile der Tabelle gibt den Wert f�r
    // GPAGMUX (MSB, 2 Bit) + GPAMUX (LSB, 2 Bit) als Dezimalzahl an
    // (siehe S. 1645 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    GpioCtrlRegs.GPAGMUX1.bit.GPIO0 = (6 >> 2);
    GpioCtrlRegs.GPAMUX1.bit.GPIO0  = (6 & 0x03);
    // GPIO 0 Pull-Up-Widerstand aktivieren
    GpioCtrlRegs.GPAPUD.bit.GPIO0 = 0;
    // GPIO 0 Asynchroner Eingang (muss f�r I2C gesetzt sein)
    GpioCtrlRegs.GPAQSEL1.bit.GPIO0 = 0x03;
    // GPIO-Sperre f�r GPIO 1 aufheben
    GpioCtrlRegs.GPALOCK.bit.GPIO1 = 0;
    // GPIO 1 auf I2C-Funktion setzen (SCL)
    GpioCtrlRegs.GPAGMUX1.bit.GPIO1 = (6 >> 2);
    GpioCtrlRegs.GPAMUX1.bit.GPIO1  = (6 & 0x03);
    // GPIO 1 Pull-Up-Widerstand aktivieren
    GpioCtrlRegs.GPAPUD.bit.GPIO1 = 0;
    // GPIO 1 Asynchroner Eingang (muss f�r I2C gesetzt sein)
    GpioCtrlRegs.GPAQSEL1.bit.GPIO1 = 0x03;

    // Takt f�r das I2C-Modul einschalten und 5 Takte
    // warten, bis der Takt zum Modul durchgestellt ist
    // (siehe S. 169 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    CpuSysRegs.PCLKCR9.bit.I2C_A = 1;
    __asm(" RPT #4 || NOP");
		// I2C-Modul ausschalten, um es zu konfigurieren
		I2caRegs.I2CMDR.bit.IRS = 0;
		// Initialisierung als Master
		I2caRegs.I2CMDR.bit.MST = 1;
		// Vorteiler auf 20 setzen (Teiler = IPSC+1) um einen I2C-Modul-Takt von 10 MHz zu erhalten
		// (siehe S. 3629 ff. Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
		// I2C-Modul-Takt = SYSCLK / (IPSC+1) = 200 MHz / (IPSC+1)
		I2caRegs.I2CPSC.bit.IPSC = 19;
		// SCL-Takt f�r 400 kHz Fastmode initialisieren.
		// Entweder ist die Gleichung (26) oder der Wert f�r d in Tabelle
		// 33-1 falsch (siehe S. 3630 Reference Manual TMS320F2838x, SPRUII0D,
		// Rev. D, July 2022). Jedenfalls lassen sich mit den beiden Informationen
		// die Werte f�r die Register I2CCLKH und I2CCLKL nicht korrekt berechnen.
		// Durch Ausprobieren ergaben sich die unteren Werte
		if (clock == I2C_CLOCK_400_KHZ)
		{
				// Zeit setzen, in der SCL 1 ist
				I2caRegs.I2CCLKH = 5;
				// Zeit setzen, in der SCL 0 ist
				I2caRegs.I2CCLKL = 4;
		}
		// Standardm��ig mit 100 kHz SCL-Takt initialisieren
		else
		{
				// Zeit setzen, in der SCL 1 ist
				I2caRegs.I2CCLKH = 42;
				// Zeit setzen, in der SCL 0 ist
				I2caRegs.I2CCLKL = 42;
		}
		// Adressformat: 7 Bit Adresse
		I2caRegs.I2CMDR.bit.XA  = 0;
		I2caRegs.I2CMDR.bit.FDF = 0;
		// Datenformat: 8 Bit Daten pro Byte
		I2caRegs.I2CMDR.bit.BC = I2C_DATA_BITS_8;
		// Sende- und Empfangs-FIFO reseten
		I2caRegs.I2CFFTX.bit.TXFFRST = 0;
		I2caRegs.I2CFFRX.bit.RXFFRST = 0;
		// FIFO-Modus einschalten (gilt sowohl f�r den Schreib- als auch den Lesebetrieb)
		I2caRegs.I2CFFTX.bit.I2CFFEN = 1;
		// Wenn der Wert im Feld I2CFFTX.TXFFST gleich oder kleiner als der Wert in
		// I2CFFTX.TXFFIL ist, wird ein Interrupt ausgel�st. Der Wert I2CFFTX.TXFFST
		// speichert die Zahl an Bytes, die im Sende-FIFO stehen. Mit jedem Byte,
		// welches vom Sende-FIFO in das Sende-Shift-Register kopiert wird, wird
		// der Wert I2CFFTX.TXFFST um 1 verringert. ACHTUNG: Der Wert spiegelt NICHT
		// die Anzahl der vollst�ndig gesendeten Bytes wieder, da sich das ent-
		// sprechende Byte noch im Sende-Shift-Register befindet und Bit f�r Bit
		// ausgesendet wird, w�hrend der Wert bereits um 1 reduziert wurde. Somit
		// wird auch der Interrupt ausgel�st, bevor das Byte vollst�ndig gesendet wurde
		I2caRegs.I2CFFTX.bit.TXFFIL = 0;
		// Wenn der Wert im Feld I2CFFRX.RXFFST gleich oder gr��er als der Wert in
		// I2CFFRX.RXFFIL ist, wird ein Interrupt ausgel�st. Der Wert I2CFFRX.RXFFST
		// speichert die Zahl an Bytes, die im Empfangs-FIFO stehen. Anders als beim
		// Sendevorgang wird der Interrupt also genau zu dem Zeitpunkt ausgel�st,
		// wenn das letzte Byte vollst�ndig empfangen wurde
		I2caRegs.I2CFFRX.bit.RXFFIL = 0;
		// Sende- und Empfangs-FIFO-Interrupt ausschalten
		I2caRegs.I2CFFTX.bit.TXFFIENA = 0;
		I2caRegs.I2CFFRX.bit.RXFFIENA = 0;
		// Sende- und Empfangs-FIFO aus den Reset holen
		I2caRegs.I2CFFTX.bit.TXFFRST = 1;
		I2caRegs.I2CFFRX.bit.RXFFRST = 1;
		// Interrupts einschalten:
		// Interrupt wenn eine STOP-Bedingung erkannt wurde
		I2caRegs.I2CIER.bit.SCD = 1;
		// Interrupt wenn ein NACK empfangen wurde
		I2caRegs.I2CIER.bit.NACK = 1;
		// I2C-Modul wieder einschalten
		I2caRegs.I2CMDR.bit.IRS = 1;

		// CPU-Interrupts w�hrend der Konfiguration global sperren
    DINT;
		// Interrupt-Service-Routinen f�r den I2C-Interrupt an die
    // entsprechende Stelle (I2CA_INT) der PIE-Vector Table speichern
    PieVectTable.I2CA_INT = &I2cISRA;
    // I2CA-Interrupt freischalten (Zeile 8, Spalte 1 der Tabelle 3-2)
    // (siehe S. 150 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    PieCtrlRegs.PIEIER8.bit.INTx1 = 1;
    // CPU-Interrupt 8 einschalten (Zeile 8 der Tabelle)
    IER |= M_INT8;
    // Interrupts global einschalten
    EINT;

		// Register-Schreibschutz setzen
		EDIS;

    // Software-Puffer inititalisieren
    I2cInitBufferReadA();
    I2cInitBufferWriteA();
    // Steuervariable f�r die Puffer-Verwaltung initialisieren
    i2cBytesToReadAfterRSA = 0;
    // Status-Flag f�r die I2C-Kommunikation auf "idle" setzen
    i2cStatusFlagA = I2C_STATUS_IDLE;
}


//=== Function: I2cInitBufferReadA ================================================================
///
/// @brief  Funktion initialisiert alle Elemente des I2C Software-Lesepuffers zu 0.
///
/// @param  void
///
/// @return void
///
//=================================================================================================
void I2cInitBufferReadA(void)
{
    for(uint16_t i=0; i<I2C_SIZE_HARDWARE_FIFO; i++)
    {
        i2cBufferReadA[i] = 0;
    }
}


//=== Function: I2cInitBufferWriteA ===============================================================
///
/// @brief  Funktion initialisiert alle Elemente des I2C Software-Schreibpuffers zu 0.
///
/// @param  void
///
/// @return void
///
//=================================================================================================
void I2cInitBufferWriteA(void)
{
    for(uint16_t i=0; i<I2C_SIZE_HARDWARE_FIFO; i++)
    {
    		i2cBufferWriteA[i] = 0;
    }
}


//=== Function: I2cGetStatusA =====================================================================
///
/// @brief	Funktion gibt den aktuellen Status der I2C-Kommunikation zur�ck. Die I2C-Kommunikation
///					ist Interrupt-basiert und kann folgende Zust�nde annehmen:
///
///					- I2C_STATUS_IDLE       : Es ist keine Kommunikation aktiv
///					- I2C_STATUS_IN_PROGRESS: Es wurde Kommunikation gestartet
///					- I2C_STATUS_FINISHED   : Eine Kommunikation ist abgeschlossen
///					- I2C_STATUS_ERROR      : Es ist ein Fehler w�hrend der Kommunikation aufgetreten
///
///					Zum starten einer Kommunikation mit einem Slave muss eine der Funktionen
///				  "I2cWriteA()", "I2cReadA()" oder "I2cWriteReadA()" aufgerufen werden
///
/// @param 	uint16_t i2cStatusFlag
///
/// @return void
///
//=================================================================================================
extern uint16_t I2cGetStatusA(void)
{
		uint16_t status = i2cStatusFlagA;
		// Solange noch keine STOP-Bedingung vollst�ndig gesendet wurde, gilt der Status der
		// Kommunikation unabh�ngig vom Wert von "i2cStatusFlagA" als "aktiv/in Bearbeitung".
		// Das Bit STP wird leicht verz�gert nach dem Senden einer STOP-Bedingung gel�scht,
		// sodass erst nach Aussendung der vollst�ndigen STOP-Bedingung die Kommunikation
		// wieder freigegeben wird
		if (I2caRegs.I2CMDR.bit.STP)
		{
				status = I2C_STATUS_IN_PROGRESS;
		}
		return status;
}


//=== Function: I2cSetStatusIdleA =================================================================
///
/// @brief	Funktion setzt das Status-Flag auf "idle" und gibt "true" zur�ck, falls die vorherige
///					Kommunikation abgeschlossen ist. Ist noch eine Kommunikation aktiv, wird das Flag nicht
///					ver�ndert und es wird "false" zur�ck gegeben.
///
/// @param	void
///
/// @return bool flagSetToIdle
///
//=================================================================================================
extern bool I2cSetStatusIdleA(void)
{
		bool flagSetToIdle = false;
		// Staus-Flag nur auf "idle" setzen, falls eine
		// vorherige Kommunikation abgeschlossen ist
		if (i2cStatusFlagA != I2C_STATUS_IN_PROGRESS)
		{
				i2cStatusFlagA = I2C_STATUS_IDLE;
				flagSetToIdle = true;
		}
		return flagSetToIdle;
}


//=== Function: I2cWriteA =========================================================================
///
/// @brief  Funktion schreibt Daten �ber I�C auf einen Slave (Master Transmitter Mode). Der erste
///					Parameter ist die 7-Bit Adresse des Slaves, an den die Daten gesendet werden sollen.
///					Der zweite Parameter gibt die Anzahl der zu sendenen Bytes (ohne die Slave-Adresse)
///					an. Die zu schreibenden Daten m�ssen von der aufrufenden Stelle in das globale Array
///					"i2cBufferWrite" kopiert werden. Es k�nnen maximal 16 Bytes geschrieben werden. Der
///					R�ckgabwert ist "true" falls die Kommunikation gestartet wurde (keine vorangegegangene
///					Kommunikation ist aktiv und der Bus ist frei), andernfalls ist er "false".
///
/// @param  uint16_t slaveAddress, uint16_t numberOfBytes
///
/// @return bool operationPerformed
///
//=================================================================================================
bool I2cWriteA(uint16_t slaveAddress,
						   uint16_t numberOfBytes)
{
		// Ergebnis des Funktionsaufrufes (Vorgang gestartet / nicht gestartet)
		bool operationPerformed = false;
		// Vorgang nur starten falls keine vorherige Kommunikation aktiv ist,
		// der Bus frei ist, zuvor eine STOP-Bedingung gesendet wurde und die
		// Anzahl der zu schreibenden Bytes die Gr��e des Software-Puffers nicht
		// �berschreitet und mindestens 1 ist
		if ((i2cStatusFlagA != I2C_STATUS_IN_PROGRESS)
				&& !I2caRegs.I2CSTR.bit.BB
				&& !I2caRegs.I2CMDR.bit.STP
				&& (numberOfBytes <= I2C_SIZE_HARDWARE_FIFO)
				&& numberOfBytes)
		{
				// R�ckgabewert auf "Operation durchgef�hrt" setzen
				operationPerformed = true;
				// Status-Flag setzen um der aufrufenden Stelle zu signalisieren,
				// dass eine I2C-Kommunikation gestartet wurde
				i2cStatusFlagA = I2C_STATUS_IN_PROGRESS;
				// Master-Transmitter Mode setzen
				I2caRegs.I2CMDR.bit.MST = 1;
				I2caRegs.I2CMDR.bit.TRX = 1;
				// Slave-Adresse setzen
				I2caRegs.I2CSAR.bit.SAR = slaveAddress;
				// Zu sendene Daten in den Sende-FIFO kopieren
				for (uint16_t i=0; i<numberOfBytes; i++)
				{
						I2caRegs.I2CDXR.bit.DATA = i2cBufferWriteA[i];
				}
				// Anzahl der zu schreibenden Bytes setzen (Adress-Byte z�hlt nicht dazu)
				I2caRegs.I2CCNT = numberOfBytes;
				// �bertragung starten
				I2caRegs.I2CMDR.bit.STT = 1;
				// STOP-Bedingung senden, nachdem alle Bytes an den Slave gesendet wurden
				I2caRegs.I2CMDR.bit.STP = 1;
		}
		return operationPerformed;
}


//=== Function: I2cReadA ==========================================================================
///
/// @brief  Funktion liest Daten �ber I�C von einem Slave (Master Receiver Mode). Der erste
///					Parameter ist die 7-Bit Adresse des Slaves, von dem die Daten gelesen werden sollen.
///					Der zweite Parameter gibt die Anzahl der zu lesenden Bytes (ohne die Slave-Adresse).
///					an. Die gelesenen Daten k�nnen von der aufrufenden Stelle aus dem globale Array
///					"i2cBufferRead" gelesen werden. Es k�nnen maximal 16 Bytes gelesen werden. Der
///					R�ckgabwert ist "true" falls die Kommunikation gestartet wurde (keine vorangegegangene
///					Kommunikation ist aktiv und der Bus ist frei), andernfalls ist er "false".
///
/// @param  uint16_t slaveAddress, uint16_t numberOfBytes
///
/// @return bool operationPerformed
///
//=================================================================================================
bool I2cReadA(uint16_t slaveAddress,
						  uint16_t numberOfBytes)
{
		// Ergebnis des Funktionsaufrufes (Vorgang gestartet/nicht gestartet)
		bool operationPerformed = false;
		// Vorgang nur starten falls keine vorherige Kommunikation aktiv ist,
		// der Bus frei ist, zuvor eine STOP-Bedingung gesendet wurde und die
		// Anzahl der zu lesenden Bytes die Gr��e des Software-Puffers nicht
		// �berschreitet und mindestens 1 ist
		if ((i2cStatusFlagA != I2C_STATUS_IN_PROGRESS)
				&& !I2caRegs.I2CSTR.bit.BB
				&& !I2caRegs.I2CMDR.bit.STP
				&& (numberOfBytes <= I2C_SIZE_HARDWARE_FIFO)
				&& numberOfBytes)
		{
				// R�ckgabewert auf "Operation durchgef�hrt" setzen
				operationPerformed = true;
				// Flag setzen um der aufrufenden Stelle zu signalisieren,
				// dass eine I2C-Kommunikation gestartet wurde
				i2cStatusFlagA = I2C_STATUS_IN_PROGRESS;
				// Master-Receiver Mode setzen
				I2caRegs.I2CMDR.bit.MST = 1;
				I2caRegs.I2CMDR.bit.TRX = 0;
				// Slave-Adresse setzen
				I2caRegs.I2CSAR.bit.SAR = slaveAddress;
				// Anzahl der zu lesenden Bytes setzen (Adress-Byte z�hlt nicht dazu)
				I2caRegs.I2CCNT = numberOfBytes;
				// �bertragung starten
				I2caRegs.I2CMDR.bit.STT = 1;
				// STOP-Bedingung senden, nachdem alle Bytes vom Slave gelesen wurden
				I2caRegs.I2CMDR.bit.STP = 1;
		}
		return operationPerformed;
}


//=== Function: I2cWriteReadA =====================================================================
///
/// @brief  Funktion schreibt und liest anschlie�end Daten �ber I�C von einem Slave (Master
///					Transmitter + Receiver Mode). Der erste Parameter ist die 7-Bit Adresse des Slaves,
///					mit dem kommuniziert werden soll. Der zweite Parameter gibt die Anzahl der zu
///					schreibenden Bytes an, der dritte Parameter die Anzahl der zu lesenden Bytes (beides
///					jeweils ohne die Slave-Adresse). Es k�nnen maximal 16 Bytes geschreiben bzw. gelesen
///					werden. Der R�ckgabwert ist "true" falls die Kommunikation gestartet wurde (keine
///					vorangegangene Kommunikation ist aktiv und der Bus ist frei), andernfalls ist er
///					"false". Die zu schreibenen Daten werden aus dem Software-Puffer "i2cBufferWriteA[]"
///					kopiert. Die gelesenen Daten werden in den Software-Puffer "i2cBufferReadA[]" kopiert.
///
/// @param  uint16_t slaveAddress, uint16_t numberOfBytesWrite, uint16_t numberOfBytesRead
///
/// @return bool operationPerformed
///
//=================================================================================================
bool I2cWriteReadA(uint16_t slaveAddress,
						       uint16_t numberOfBytesWrite,
									 uint16_t numberOfBytesRead)
{
		// Ergebnis des Funktionsaufrufes (Vorgang gestartet / nicht gestartet)
		bool operationPerformed = false;
		// Vorgang nur starten falls keine vorherige Kommunikation aktiv ist,
		// der Bus frei ist, zuvor eine STOP-Bedingung gesendet wurde und die
		// Anzahl der zu schreibenden und lesenden Bytes die Gr��e der Software-
		// Puffer nicht �berschreitet und mindesten 1 ist
		if ((i2cStatusFlagA != I2C_STATUS_IN_PROGRESS)
				&& !I2caRegs.I2CSTR.bit.BB
				&& !I2caRegs.I2CMDR.bit.STP
				&& (numberOfBytesWrite <= I2C_SIZE_HARDWARE_FIFO)
				&& (numberOfBytesRead  <= I2C_SIZE_HARDWARE_FIFO)
				&& numberOfBytesWrite
				&& numberOfBytesRead)
		{
				// R�ckgabewert auf "Operation durchgef�hrt" setzen
				operationPerformed = true;
				// Flag setzen um der aufrufenden Stelle zu signalisieren,
				// dass eine I2C-Kommunikation gestartet wurde
				i2cStatusFlagA = I2C_STATUS_IN_PROGRESS;
				// Master-Transmitter Mode setzen
				I2caRegs.I2CMDR.bit.MST = 1;
				I2caRegs.I2CMDR.bit.TRX = 1;
				// Slave-Adresse setzen
				I2caRegs.I2CSAR.bit.SAR = slaveAddress;
				// Zu sendene Daten in den TX-FIFO kopieren
				for (uint16_t i=0; i<numberOfBytesWrite; i++)
				{
						I2caRegs.I2CDXR.bit.DATA = i2cBufferWriteA[i];
				}
				// Anzahl der zu schreibenden Bytes setzen (Adress-Byte z�hlt nicht dazu)
				I2caRegs.I2CCNT = numberOfBytesWrite;
				// Anzahl der zu lesenden Bytes setzen (Adress-Byte z�hlt nicht dazu).
				// Dieser Wert wird nach Ende der Schreib-Operation in der ISR in das
				// Register I2CCNT geschrieben
				i2cBytesToReadAfterRSA = numberOfBytesRead;
				// ARDY-Interrupt einschalten damit das Ende der
				// Schreib-Operation detektiert werden kann
				I2caRegs.I2CIER.bit.ARDY = 1;
				// �bertragung starten
				I2caRegs.I2CMDR.bit.STT = 1;
		}
		return operationPerformed;
}


//=== Function: I2cISRA ===========================================================================
///
/// @brief	Funktion wird aufgerufen, wenn eine STOP-Benung auf dem Bus gesendet, ein NACK
///					empfangen, ein Byte vom Empfangs-Register in den (Hardware) Empfangs-Puffer geshiftet,
///					ein Byte vom (Hardware) Sende-Puffer in das Sende-Register geshiftet oder der interne
///					Z�hler des I2C-Moduls, welcher die Bytes z�hlt, auf 0 gegangen ist und keine STOP-
///					Bedingung gesendet wurde.
///
/// @param  void
///
/// @return void
///
//=================================================================================================
__interrupt void I2cISRA(void)
{
		// Bei jedem Eintritt in eine Interrupt-Service-Routine (ISR) wird automatisch
		// das EALLOW-Bit gel�scht, unabh�ngig davon, ob es zuvor gesetzt war (siehe
		// S. 148 Punkt 9, Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022).
		// Nach Abschluss der ISR wird das EALLOW-Bit automatisch wieder gesetzt, falls
		// es vor dem Auftritt des Interrupts gesetzt war. Falls in der ISR nur auf
		// Register ohne Schreibschutz zugegriffen wird, kann auf den folgenden Befehl
		// verzichtet werden (siehe Spalte "Write Protection" in der Register�bersicht)
		//EALLOW;

		// Es wurde eine STOP-Bedingung erkannt
		if (I2caRegs.I2CSTR.bit.SCD)
		{
				// STOP-Flag l�schen
				I2caRegs.I2CSTR.bit.SCD = 1;
				// Daten auslesen, fallsReceiver-Mode aktiv
				if (!I2caRegs.I2CMDR.bit.TRX)
				{
						// Empfangene Daten auslesen
						for (uint16_t i=0; i<I2caRegs.I2CCNT; i++)
						{
								i2cBufferReadA[i] = I2caRegs.I2CDRR.bit.DATA;
						}
				}
				// Status-Flag setzen um das Ende der �bertragung zu
				// signalisieren, falls kein Fehler aufgetreten ist
				if (i2cStatusFlagA == I2C_STATUS_IN_PROGRESS)
				{
						i2cStatusFlagA = I2C_STATUS_FINISHED;
				}
		}
		// Ein NACK wurde empfangen. Im Master-Receiver-Mode
		// nur m�glich nach dem Senden der Slave-Adresse
		else if (I2caRegs.I2CSTR.bit.NACK)
		{
				// NACK-Flag l�schen
				I2caRegs.I2CSTR.bit.NACK = 1;
				// STOP-Bedingung senden
				I2caRegs.I2CMDR.bit.STP = 1;
				// Status-Flag setzen um ein Fehler in der �bertragung zu signalisieren
				i2cStatusFlagA = I2C_STATUS_ERROR;
		}
		// Write-Read-Modus:
		// Im Non-Repeat Mode (RM in I2CMDR ist nicht gesetzt) wird ARDY gesetzt, sobald
		// die Anzahl von Bytes, die in I2CCNT steht, geschrieben bzw. gelesen wurde und keine
		// STOP-Bedingung abgesendet wurde (STP in I2CMDR wurde nach dem Start nicht gesetzt)
		// oder wenn ein NACK empfangen wurde. Damit letztgenannte Bedingung im Fehlerfall
		// (z.B. ung�ltige Slave-Adresse) nicht dazu f�hrt, dass st�ndig eine wiederholte
		// START-Bedingung getriggert wird, wird zus�tzlich abgefragt, ob ein NACK empfangen
		// wurde
		if (I2caRegs.I2CSTR.bit.ARDY
				&& !I2caRegs.I2CSTR.bit.NACK)
		{
				// ARDY-Interrupt ausschalten (dieser wird nur einmalig
				// bei einer Schreib-Lese-Operation ben�tigt um das Ende
				// des Schreib-Datenpakets zu detektieren)
				I2caRegs.I2CIER.bit.ARDY = 0;
				// Master-Receiver Mode setzen
				I2caRegs.I2CMDR.bit.MST = 1;
				I2caRegs.I2CMDR.bit.TRX = 0;
				// Anzahl der zu lesenden Bytes setzen (Adress-Byte z�hlt nicht dazu)
				I2caRegs.I2CCNT = i2cBytesToReadAfterRSA;
				// Wiederholte START-Bedingung senden
				I2caRegs.I2CMDR.bit.STT = 1;
				// STOP-Bedingung senden, nachdem alle Bytes vom Slave gelesen wurden
				I2caRegs.I2CMDR.bit.STP = 1;
		}

		// Interrupt-Flag der Gruppe 8 l�schen (da geh�rt der INT_I2CA-Interrupt zu)
		PieCtrlRegs.PIEACK.bit.ACK8 = 1;
}
