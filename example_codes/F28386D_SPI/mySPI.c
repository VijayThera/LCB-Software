//=================================================================================================
/// @file       mySPI.c
///
/// @brief      Datei enth�lt Variablen und Funktionen um die SPI-Schnittstelle als Master zu
///							benutzen. Die Kommunikation ist Interrupt-basiert und nutzt die beiden 16x16-Bit
///						  gro�en Hardware-FIFOs. Pro Kommunikationvorgang k�nnen bis zu 65.536 Bytes
///							�bertragen werden. Der Hardware-FIFO wird dabei so oft mit den Daten aus dem
///							Software-Puffer beschrieben bzw. Daten aus dem FIFO in den Software-Puffer
///							geschrieben, bis die gew�nschte Zahl an Btes �bertragen wurde.Der Status der
///							Kommunikation (aktiv oder nicht aktiv) kann �ber die Funktion "SpiGetStatus()"
///							abgefragt werden.
///
///							�nderung in Version 2.0: Verwendung der Hardware-FIFOs
///
/// @version    V2.0
///
/// @date       24.03.2023
///
/// @author     Daniel Urbaneck
//=================================================================================================
//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------
#include "mySPI.h"


//-------------------------------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------------------------------
// Software-Puffer f�r die SPI-Kommunikation
uint16_t spiBufferTxA[SPI_SIZE_SOFTWARE_BUFFER];
uint16_t spiBufferRxA[SPI_SIZE_SOFTWARE_BUFFER];
// Steuern das Kopieren in und aus den Software-Puffern w�hrend der SPI-Kommunikation
uint16_t spiBufferIndexTxA;
uint16_t spiBufferIndexRxA;
uint16_t spiBytesToTransferA;
// Flag speichert den aktuellen Zustand der SPI-Kommunikation
uint16_t spiStatusFlagA;


//-------------------------------------------------------------------------------------------------
// Global functions
//-------------------------------------------------------------------------------------------------
//=== Function: SpiInitA ==========================================================================
///
/// @brief  Funktion initialisiert GPIO 54 (MOSI), GPIO 55 (MISO), GPIO 56 (CLK) und GPIO 58 (SS)
/// 				als SPI-Pins und das SPI-A Modul als Master mit Taktrate nach �bergebenen Funktions-
///					parameter "clock" und 8 Bit Datenl�nge. Der SPI-Interrupt wird eingeschaltet und die
///					ISR auf die PIE-Vector-Tabelle gesetzt. Die Konfiguration der SPI Module B, C und D
///					sind analog zu dem hier gezeigten Modul A.
///
/// @param  uint32_t clock
///
/// @return void
///
//=================================================================================================
void SpiInitA(uint32_t clock)
{
    // Register-Schreibschutz aufheben
    EALLOW;

    // GPIO-Sperre f�r GPIO 54 (MOSI), 55 (MISO), 56 (CLK) und 58 (SS) aufheben
    GpioCtrlRegs.GPBLOCK.bit.GPIO54  = 0;
    GpioCtrlRegs.GPBLOCK.bit.GPIO55 = 0;
    GpioCtrlRegs.GPBLOCK.bit.GPIO56 = 0;
    GpioCtrlRegs.GPBLOCK.bit.GPIO58 = 0;
    // GPIO 54 auf SPI-Funktion setzen (MOSI).
    // Die Zahl in der obersten Zeile der Tabelle gibt den Wert f�r
    // GPAGMUX (MSB, 2 Bit) + GPAMUX (LSB, 2 Bit) als Dezimalzahl an
    // (siehe S. 1647 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    GpioCtrlRegs.GPBGMUX2.bit.GPIO54 = (1 >> 2);
    GpioCtrlRegs.GPBMUX2.bit.GPIO54  = (1 & 0x03);
    // GPIO 54 Pull-Up-Widerstand deaktivieren
    GpioCtrlRegs.GPBPUD.bit.GPIO54 = 1;
    // GPIO 54 Asynchroner Eingang (muss f�r SPI gesetzt sein)
    GpioCtrlRegs.GPBQSEL2.bit.GPIO54 = 0x03;
    // GPIO 55 auf SPI-Funktion setzen (MISO)
    GpioCtrlRegs.GPBGMUX2.bit.GPIO55 = (1 >> 2);
    GpioCtrlRegs.GPBMUX2.bit.GPIO55  = (1 & 0x03);
    // GPIO 55 Pull-Up-Widerstand deaktivieren
    GpioCtrlRegs.GPBPUD.bit.GPIO55 = 1;
    // GPIO 55 Asynchroner Eingang (muss f�r SPI gesetzt sein)
    GpioCtrlRegs.GPBQSEL2.bit.GPIO55 = 0x03;
    // GPIO 56 auf SPI-Funktion setzen (CLK)
    GpioCtrlRegs.GPBGMUX2.bit.GPIO56 = (1 >> 2);
    GpioCtrlRegs.GPBMUX2.bit.GPIO56  = (1 & 0x03);
    // GPIO 56 Pull-Up-Widerstand deaktivieren
    GpioCtrlRegs.GPBPUD.bit.GPIO56 = 1;
    // GPIO 56 Asynchroner Eingang (muss f�r SPI gesetzt sein)
    GpioCtrlRegs.GPBQSEL2.bit.GPIO56 = 0x03;
    // GPIO 58 auf GPIO-Funktionalit�t setzen (SS Slave 1)
    GpioCtrlRegs.GPBGMUX2.bit.GPIO58 = (0 >> 2);
    GpioCtrlRegs.GPBMUX2.bit.GPIO58  = (0 & 0x03);
    // GPIO 58 Pull-Up-Widerstand deaktivieren
    GpioCtrlRegs.GPBPUD.bit.GPIO58 = 1;
    // Zustand von GPIO 58 auf high setzen
    GpioDataRegs.GPBSET.bit.GPIO58 = 1;
    // GPIO 58 als Ausgang setzen
    GpioCtrlRegs.GPBDIR.bit.GPIO58 = 1;
    // GPIO 59 auf GPIO-Funktionalit�t setzen (SS Slave 2)
    GpioCtrlRegs.GPBGMUX2.bit.GPIO59 = (0 >> 2);
    GpioCtrlRegs.GPBMUX2.bit.GPIO59  = (0 & 0x03);
    // GPIO 59 Pull-Up-Widerstand deaktivieren
    GpioCtrlRegs.GPBPUD.bit.GPIO59 = 1;
    // Zustand von GPIO 59 auf high setzen
    GpioDataRegs.GPBSET.bit.GPIO59 = 1;
    // GPIO 59 als Ausgang setzen
    GpioCtrlRegs.GPBDIR.bit.GPIO59 = 1;


    // Takt f�r das SPI-Modul einschalten und 5 Takte
    // warten, bis der Takt zum Modul durchgestellt ist
    // (siehe S. 169 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    CpuSysRegs.PCLKCR8.bit.SPI_A = 1;
    __asm(" RPT #4 || NOP");
		// SPI-Modul zum konfigurieren ausschalten
    SpiaRegs.SPICCR.bit.SPISWRESET = 0;
    // Polarit�t = 0 (Ruhepegel: CLK = 0)
    SpiaRegs.SPICCR.bit.CLKPOLARITY = 0;
    // Phase = 0 (Daten bei der ersten (mit POL = 0 also einer steigenden) Flanke �bernehmen)
    SpiaRegs.SPICTL.bit.CLK_PHASE = 0;
    // 8-Bit Datenl�nge
    SpiaRegs.SPICCR.bit.SPICHAR = 7;
    // Master-Mode setzen
    SpiaRegs.SPICTL.bit.MASTER_SLAVE = 1;
    // �bertragung aktivieren
    SpiaRegs.SPICTL.bit.TALK = 1;
    // Taktrate setzen
    // (Low-Speed CLK / clock) - 1
    // Low-Speed CLK = 50 MHz (siehe "DeviceInit()")
    SpiaRegs.SPIBRR.bit.SPI_BIT_RATE = (50000000 / clock) - 1;
    // FIFO-Reset w�hrend der Konfiguration setzen
    SpiaRegs.SPIFFTX.bit.TXFIFO = 0;
    // FIFO-Modus einschalten
    SpiaRegs.SPIFFTX.bit.SPIFFENA = 1;
    // RX-FIFO Interrupt ausschalten
    SpiaRegs.SPIFFRX.bit.RXFFIENA = 0;
    // RX-FIFO Interupt-Flag l�schen
    SpiaRegs.SPIFFRX.bit.RXFFINTCLR = 1;
    // FIFO-Reset aufheben
    SpiaRegs.SPIFFTX.bit.TXFIFO = 1;
    // SPI-Modul wieder einschalten
    SpiaRegs.SPICCR.bit.SPISWRESET = 1;

    // CPU-Interrupts w�hrend der Konfiguration global sperren
    DINT;
    // Interrupt-Service-Routinen f�r den SPI-Interrupt an die
    // entsprechende Stelle (SPIA_RX_INT) der PIE-Vector Table speichern
    PieVectTable.SPIA_RX_INT = &SpiISRA;
    // SPIA_RX-Interrupt freischalten (Zeile 6, Spalte 1 der Tabelle 3-2)
    // (siehe S. 150 Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022)
    PieCtrlRegs.PIEIER6.bit.INTx1 = 1;
    // CPU-Interrupt 6 einschalten (Zeile 6 der Tabelle)
    IER |= M_INT6;
    // CPU-Interrupts nach Konfiguration global wieder freigeben
    EINT;

		// Register-Schreibschutz setzen
		EDIS;

    // Steuervariablen initialisieren
    SpiInitBufferRxA();
    SpiInitBufferTxA();
    spiBufferIndexTxA = 0;
    spiBufferIndexRxA = 0;
    spiBytesToTransferA = 0;
    spiStatusFlagA = SPI_STATUS_IDLE;
}


//=== Function: SpiInitBufferTxA ==================================================================
///
/// @brief	Funktion setzte alle Elemente des Sende-Software-Puffers "spiBufferTxA[]" zu 0
///
/// @param	void
///
/// @return void
///
//=================================================================================================
void SpiInitBufferTxA(void)
{
		// Alle Elemente zu 0 setzen
		for (uint16_t i=0; i<SPI_SIZE_SOFTWARE_BUFFER; i++)
		{
				spiBufferTxA[i] = 0;
		}
}


//=== Function: SpiInitBufferRxA ==================================================================
///
/// @brief	Funktion setzte alle Elemente des Empfangs-Software-Puffers "spiBufferRxA[]" zu 0
///
/// @param	void
///
/// @return void
///
//=================================================================================================
void SpiInitBufferRxA(void)
{
		// Alle Elemente zu 0 setzen
		for (uint16_t i=0; i<SPI_SIZE_SOFTWARE_BUFFER; i++)
		{
				spiBufferRxA[i] = 0;
		}
}


//=== Function: SpiGetStatusA =====================================================================
///
/// @brief	Funktion gibt den aktuellen Status der SPI-Kommunikation zur�ck. Die SPI-
///					Kommunikation ist Interrupt-basiert und kann folgende Zust�nde annehmen:
///
///					- SPI_STATUS_IDLE       : Es ist keine Kommunikation aktiv
///					- SPI_STATUS_IN_PROGRESS: Es wurde eine Kommunikation gestartet
///					- SPI_STATUS_FINISHED   : Eine Kommunikation ist abgeschlossen
///
///					Zum starten einer Kommunikation muss die Funktion "SpiSendDataA()"
///					aufgerufen werden
///
/// @param 	uint16_t spiStatusFlag
///
/// @return void
///
//=================================================================================================
extern uint16_t SpiGetStatusA(void)
{
		return spiStatusFlagA;
}


//=== Function: SpiSetStatusIdleA =================================================================
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
extern bool SpiSetStatusIdleA(void)
{
		bool flagSetToIdle = false;
		// Staus-Flag nur auf "idle" setzen, falls eine
		// vorherige Kommunikation abgeschlossen ist
		if (spiStatusFlagA == SPI_STATUS_FINISHED)
		{
				spiStatusFlagA = SPI_STATUS_IDLE;
				flagSetToIdle = true;
		}
		return flagSetToIdle;
}


//=== Function: SpiSendDataA ======================================================================
///
/// @brief  Funktion sendet Daten �ber SPI. Der Parameter "slave" gibt an, welche SS-Leitung
///					aktiviert werden soll. Nach dem letzten gesendeten Byte (numberOfBytes), wird ein
///					Interrupt ausgel�st um die empfangenen Daten auslesen zu k�nnen. Sollen Bytes nur
///				  empfangen werden (Slave abfragen), so wird die Byte-Anzahl �ber den Parameter
///					"numberOfBytes" �bergeben und das Array "spiBufferTxA" mit Dummy-Daten gef�llt.
///
/// @param  uint16_t slave, uint16_t numberOfBytes
///
/// @return bool operationPerformed
///
//=================================================================================================
bool SpiSendDataA(uint16_t slave,
									uint16_t numberOfBytes)
{
		// Ergebnis des Funktionsaufrufes (Kommunikation gestartet / nicht gestartet)
		bool operationPerformed = false;
		// Vorgang nur starten falls keine vorherige Kommunikation aktiv
		// ist und die Anzahl der zu sendenen / empfangenen Bytes die
		// Gr��e der Software-Puffer nicht �berschreitet und mindestens 1 ist
		if (   (spiStatusFlagA != SPI_STATUS_IN_PROGRESS)
				&& (numberOfBytes <= SPI_SIZE_SOFTWARE_BUFFER)
				&&  numberOfBytes)
		{
				// Slave ausw�hlen
				if (slave == SPI_SLAVE_1)
				{
						SPI_DISABLE_SLAVE_2;
						SPI_ENABLE_SLAVE_1;
				}
				else if (slave == SPI_SLAVE_2)
				{
						SPI_DISABLE_SLAVE_1;
						SPI_ENABLE_SLAVE_2;
				}
				// Ggf. kurz warten, bis der Slave bereit ist
				//DELAY_US(1);
				// Flag setzen um der aufrufenden Stelle zu signalisieren,
				// dass die SPI-Kommunikation gestartet wurde
				spiStatusFlagA = SPI_STATUS_IN_PROGRESS;
				// Anzahl der zu sendenen Bytes an die Steuervariable �bergeben.
				// Diese koordiniert die restliche Kommunikation in der ISR
				spiBytesToTransferA = numberOfBytes;
				// Indexe zur Verwaltung der Software-Puffer auf das erste Element
				// setzen, damit die zu sendenen Daten vom Anfang des Sende-Puffers
				// kopiert bzw. die empfangenen Daten an den Anfang des Empfangs-
				// Puffers geschrieben werden
				spiBufferIndexTxA = 0;
				spiBufferIndexRxA = 0;
				// Zu sendene Daten von dem Software-Puffer in den SPI Sende-FIFO kopieren bis dieser
				// gef�llt ist oder der Software-Puffer leer ist. Dieser ist Datenbreite*16 Bit gro�.
				// Die Datenbreite kann zwischen 1 und 16 eingestellt werden (siehe S. 3904 Reference
				// Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022). Die Reihenfolge, in der die Daten
		    // ausgesendet werden ist:
				// MSB: i = 0
				// LSB: i = numberOfBytes-1
				while (   (spiBufferIndexTxA < spiBytesToTransferA)
							 &&	(SpiaRegs.SPIFFTX.bit.TXFFST < SPI_SIZE_HARDWARE_FIFO))
				{
						// Daten in den SPI-Hardware-Puffer kopieren. Daten m�ssen dabei linksb�nig sein (S. 3915
						// Reference Manual TTMS320F2838x, SPRUII0D, Rev. D, July 2022), da nur die Anzahl an Bits
						// ausgesendet werden, die als Datenl�nge festgelegt wurden (SPICHAR + 1). Sobald das erste
						// Element in den FIFO-Puffer geschrieben wurde, beginnt der Sendevorgang
						SpiaRegs.SPITXBUF = (spiBufferTxA[spiBufferIndexTxA] << 8);
						spiBufferIndexTxA++;
				}
				// Interrupt ausl�sen, wenn "numberOfBytes" Bytes oder die maximale
				// Anzahl an Bytes des FIFO-Empfangspuffers empfangen wurden, je nach
				// dem was kleiner ist
				SpiaRegs.SPIFFRX.bit.RXFFIL = spiBytesToTransferA;
				if (spiBytesToTransferA > SPI_SIZE_HARDWARE_FIFO)
				{
						SpiaRegs.SPIFFRX.bit.RXFFIL = SPI_SIZE_HARDWARE_FIFO;
				}
				// RX-FIFO Interrupt einschalten
				SpiaRegs.SPIFFRX.bit.RXFFIENA = 1;
		}
		return operationPerformed;
}


//=== Function: SpiISRA ===========================================================================
///
/// @brief	Funktion wird aufgerufen, sobald die im Register SPIFFRX.bit.RXFFIL stehende
///					Anzahl an Bytes �ber SPI empfangen wurde. F�r den reinen Sendebetrieb k�nnte
///					zwar der SPI_TX_INT verwendet werden, aber die Konfiguration des Sende-Interrupts
///					ist komplizierter als die des Emfangs-Interrupts. Zudem wird der Sende-Interrupt
///					ausgel�st, sobald der TX-Puffer leer ist. Zu diesem Zeitpunkt befindet sich das
///					letzte zu sendene Byte jedoch noch im Ausgangsregister. Der Interrupt kommt also
///					zu fr�h. Aus diesem  Grund und weil beim Senden auch automatisch Daten empfangen
///					werden, wurde hier die Kommunikation ausschlie�lich mithilfe des Empfangs-Interrupts
///					umgesetzt.
///
/// @param  void
///
/// @return void
///
//=================================================================================================
__interrupt void SpiISRA(void)
{
		// Bei jedem Eintritt in eine Interrupt-Service-Routine (ISR) wird automatisch
		// das EALLOW-Bit gel�scht, unabh�ngig davon, ob es zuvor gesetzt war (siehe
		// S. 148 Punkt 9, Reference Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022).
		// Nach Abschluss der ISR wird das EALLOW-Bit automatisch wieder gesetzt, falls
		// es vor dem Auftritt des Interrupts gesetzt war. Falls in der ISR nur auf
		// Register ohne Schreibschutz zugegriffen wird, kann auf den folgenden Befehl
		// verzichtet werden (siehe Spalte "Write Protection" in der Register�bersicht)
		//EALLOW;

		// Daten aus dem Empfangs-FIFO in den Software-Puffer kopieren bis die
		// komplette Anzahl an Bytes empfangen wurde oder der Empfangs-FIFO leer ist
		while (   (spiBufferIndexRxA < spiBytesToTransferA)
					 && (SpiaRegs.SPIFFRX.bit.RXFFST > 0))
		{
				spiBufferRxA[spiBufferIndexRxA] = SpiaRegs.SPIRXBUF;
				spiBufferIndexRxA++;
		}
		// Komplette Anzahl an Bytes wurde gesendet/empfangen
		if (spiBufferIndexRxA == spiBytesToTransferA)
		{
				// Empfangs-FIFO-Interrupt ausschalten
		    SpiaRegs.SPIFFRX.bit.RXFFIENA = 0;
				// Slaves abw�hlen
				SPI_DISABLE_SLAVE_1;
				SPI_DISABLE_SLAVE_2;
				// Flag setzen um das Ende der �bertragung zu signalisieren
				spiStatusFlagA = SPI_STATUS_FINISHED;
		}
		// Es sollen noch weitere Bytes gesendet/empfangen werden
		else
		{
				// Die n�chsten Daten in den Sende-FIO kopieren. Dieser ist Datenbreite*16 Bit gro�.
				// Die Datenbreite kann zwischen 1 und 16 eingestellt werden (siehe S. 3904 Reference
				// Manual TMS320F2838x, SPRUII0D, Rev. D, July 2022). Die Reihenfolge, in der die Daten
				// ausgesendet werden ist:
				// MSB: i = 0
				// LSB: i = numberOfBytes-1
				while (   (spiBufferIndexTxA < spiBytesToTransferA)
							 &&	(SpiaRegs.SPIFFTX.bit.TXFFST < SPI_SIZE_HARDWARE_FIFO))
				{
						// Daten in den SPI-Hardware-Puffer kopieren. Daten m�ssen dabei linksb�nig sein (S. 3915
						// Reference Manual TTMS320F2838x, SPRUII0D, Rev. D, July 2022), da nur die Anzahl an Bits
						// ausgesendet werden, die als Datenl�nge festgelegt wurden (SPICHAR + 1). Sobald das erste
						// Element in den FIFO-Puffer geschrieben wurde, beginnt der Sendevorgang
						SpiaRegs.SPITXBUF = (spiBufferTxA[spiBufferIndexTxA] << 8);
						spiBufferIndexTxA++;
				}
				// Interrupt ausl�sen, wenn "spiBytesToTransferA - spiBufferIndexRxA" Bytes oder
				// die maximale Anzahl an Bytes des FIFO-Empfangspuffers empfangen wurden, je nach
				// dem was kleiner ist
				SpiaRegs.SPIFFRX.bit.RXFFIL = spiBytesToTransferA - spiBufferIndexRxA;
				if ((spiBytesToTransferA - spiBufferIndexRxA) > SPI_SIZE_HARDWARE_FIFO)
				{
						SpiaRegs.SPIFFRX.bit.RXFFIL = SPI_SIZE_HARDWARE_FIFO;
				}
		}

    // RX-FIFO Interupt-Flag l�schen
    SpiaRegs.SPIFFRX.bit.RXFFINTCLR = 1;
		// Interrupt-Flag der Gruppe 6 l�schen (da geh�rt der SPI-Interrupt zu)
    PieCtrlRegs.PIEACK.bit.ACK6 = 1;
}


