//++
// PPI.hpp -> CPPI Generic Parallel Interface emulation
//
// DESCRIPTION:
//   This class implements a generic emulation of a parallel interface chip,
// such as the Intel 8255, RCA CDP1854, National NSC810 or INS8154, etc.  It's
// not all that useful by itself, but is intended to be used as a base class
// for system specific emulations.
//
// REVISION HISTORY:
// 28-JUN-23  RLA   New file.
// 25-MAR-25  RLA   Add EnablePPI() ...
//                  Setting the STROBE/READY outputs in bit programmable mode
//                  is a separate command byte from the mode set!
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Device.hpp"           // generic device definitions
using std::string;              // ...
class CEventQueue;		// ...

// Specific PPI types ...
enum _PPI_TYPES {
  PPI_UNKNOWN   =    0,     // undefined
  PPI_I8255     = 8255,     // ubiquitous Intel 8255/6
  PPI_I8155     = 8155,	    // Intel 8155/6 RAM-I/O-TIMER
  PPI_CDP1851   = 1851,     // RCA CDP1851 programmable I/O interface
  PPI_NSC810	=  810,	    // National NSC810 (partner to the NSC800)
  PPI_INS8154   = 8154,	    // National INS8154 (partner to the SC/MP)
};
typedef enum _PPI_TYPES PPI_TYPE;


class CPPI : public CDevice {
  //++
  // Generic "Programmable peripheral interface" emulation ...
  //--

  // Local definitions
public:
  // Possible modes for I/O ports ...
  enum _PORT_MODES {
    SIMPLE_INPUT,           // simple input mode
    SIMPLE_OUTPUT,          // simple output mode
    STROBED_INPUT,          // input with handshaking
    STROBED_OUTPUT,         // output with handshaking
    BIDIRECTIONAL,          // full bidirectional with handshaking
    BIT_PROGRAMMABLE,       // individually programmable per bit
  };
  typedef enum _PORT_MODES PORT_MODE;

  // Constructor and destructor ...
public:
  CPPI (const char *pszName, const char *pszType, address_t nPort, address_t cPorts, CEventQueue *pEvents=NULL);
  virtual ~CPPI() {};
private:
  // Disallow copy and assignments!
  CPPI (const CPPI &) = delete;
  CPPI& operator= (CPPI const &) = delete;

  // Public properties ...
public:
  // Return the specific PPI subtype ...
  virtual PPI_TYPE GetType() const = 0;
  //   Enable or disable the PPI. If the PPI is disabled, then it's as if
  // the PPI chip doesn't exist in the target system.
  void EnablePPI (bool fEnable) {m_fEnablePPI = fEnable;}
  bool IsPPIenabled() const {return m_fEnablePPI;}
  // Get or set the modes of ports A, B or C ...
  virtual PORT_MODE GetModeA() const {return m_ModeA;}
  virtual PORT_MODE GetModeB() const {return m_ModeB;}
  virtual PORT_MODE GetModeC() const {return m_ModeC;}
  virtual void SetModeA (PORT_MODE mode, bool fDDR=true);
  virtual void SetModeB (PORT_MODE mode, bool fDDR=true);
  virtual void SetModeC (PORT_MODE mode, bool fDDR=true);
  // Get or set the DDR bits for each port ...
  virtual void SetDDRA (uint8_t bDDR) {m_bDDRA = bDDR;}
  virtual uint8_t GetDDRA() const {return m_bDDRA;}
  virtual void SetDDRB (uint8_t bDDR) {m_bDDRB = bDDR;}
  virtual uint8_t GetDDRB() const {return m_bDDRB;}
  virtual void SetDDRC (uint8_t bDDR) {m_bDDRC = bDDR;}
  virtual uint8_t GetDDRC() const {return m_bDDRC;}
  // Get or set the port width mask (port C only) ...
  virtual void SetMaskC (uint8_t bMask) {m_bMaskC = bMask;}
  virtual uint8_t GetMaskC() const {return m_bMaskC;}
  // Return the current IBF/OBE flags ...
  virtual bool GetIBFA() const {return m_fIBFA;}
  virtual bool GetIBFB() const {return m_fIBFB;}
  virtual bool GetOBEA() const {return m_fOBEA;}
  virtual bool GetOBEB() const {return m_fOBEB;}
  // Return the current IEN/IRQ status ...
  virtual bool GetIENA() const {return m_fIENA;}
  virtual bool GetIENB() const {return m_fIENB;}
  virtual bool GetIRQA() const {return m_fIRQA;}
  virtual bool GetIRQB() const {return m_fIRQB;}
  // Set the interrupt enable bits ...
  virtual void SetIENA (bool fIEN) {m_fIENA = fIEN;  UpdateInterrupts();}
  virtual void SetIENB (bool fIEN) {m_fIENB = fIEN;  UpdateInterrupts();}

  // Device methods inherited from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual uint8_t DevRead (address_t nPort) override = 0;
  virtual void DevWrite (address_t nPort, uint8_t bData) override = 0;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Simple, non-strobed, I/O emulation ...
public:
  //   These methods are called whenever a new byte is written to ports A, B
  // or C in simple, mode 0, output mode.
  virtual void OutputA (uint8_t bNew) {};
  virtual void OutputB (uint8_t bNew) {};
  virtual void OutputC (uint8_t bNew) {};
  //   And these methods are called whenever the simulation tries to read a
  // byte in simple, mode 0, input mode.  Whatever they return is what will be
  // passed to the simulated software.
  virtual uint8_t InputA() {return 0xFF;}
  virtual uint8_t InputB() {return 0xFF;}
  virtual uint8_t InputC() {return 0xFF;}

  // Strobed input/output emulation ...
public:
  // These are the methods we call which a derived class should override ...
  virtual void StrobedOutputA (uint8_t bData) {};
  virtual void StrobedOutputB (uint8_t bData) {};
  virtual void InputReadyA() {};
  virtual void InputReadyB() {};
  // These are the methods we implement which a derived class should call ...
  virtual void StrobedInputA (uint8_t bData);
  virtual void StrobedInputB (uint8_t bData);
  virtual void OutputDoneA();
  virtual void OutputDoneB();

  // Interrupt emulation ...
protected:
  // Update the current interrupt request status ...
  virtual void UpdateInterrupts();

  // Local methods ...
protected:
  // Mask input or output bits according to the DDR ..
  static inline uint8_t MaskInput  (uint8_t bInput,  uint8_t bMask) {return bInput  & ~bMask;}
  static inline uint8_t MaskOutput (uint8_t bOutput, uint8_t bMask) {return bOutput &  bMask;}
  // Combine input and output bits under DDR mask ...
  static inline uint8_t MaskIO (uint8_t bInput, uint8_t bOutput, uint8_t bMask)
    {return (bInput & ~bMask) | (bOutput & bMask);}
  // Handle reads or writes for ports A, B or C...
  virtual uint8_t ReadA();
  virtual uint8_t ReadB();
  virtual uint8_t ReadC();
  virtual void WriteA (uint8_t bData);
  virtual void WriteB (uint8_t bData);
  virtual void WriteC (uint8_t bData);
  // Convert a port mode to a string ...
  static const char *ModeToString (PORT_MODE nMode);

  // Private member data...
protected:
  bool    m_fEnablePPI;             // TRUE if the PPI chip exists
  //   Ports A and B both potentially have latches for both input and output.
  // Port A is fully bidirectional and can be strobed either way.  Port B is
  // unidirectional, but it can still do strobed transfers either direction.
  uint8_t m_bInputA, m_bOutputA;    // current port A contents
  uint8_t m_bInputB, m_bOutputB;    //  "   "    "  B   "  "
  uint8_t m_bInputC, m_bOutputC;    //  "   "    "  C   "  "
  //   These are the input buffer full and output buffer empty flags for each
  // of the A and B ports.  We keep separate input and output flags to allow
  // for bidirectional operation.  Note that port C does not support strobed
  // (nor bidirectional) operation, so no flag is needed for it.
  bool    m_fIBFA, m_fIBFB;         // input buffer full
  bool    m_fOBEA, m_fOBEB;         // output buffer empty
  //   Data direction registers (DDR) for ports A, B and C.  A one bit in the
  // DDR means an output pin...
  uint8_t m_bDDRA, m_bDDRB, m_bDDRC;// data direction A, B and C
  // Mask for port C in the event that it has fewer than 8 bits ...
  uint8_t m_bMaskC;                 // mask of implemented bits
  // Interrupt enable and interrupt request flags for ports A and B ...
  bool    m_fIENA, m_fIENB;         // interrupt enables ...
  bool    m_fIRQA, m_fIRQB;         // ... and interrupt requests
  // Port mode associated with each of the three ports ...
  PORT_MODE m_ModeA, m_ModeB, m_ModeC;
};
