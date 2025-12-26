# Station Controller - ATU Integration Version

[![Branch](https://img.shields.io/badge/branch-switch+atu-blue.svg)](https://github.com/frank-ydf/station-controller/tree/switch+atu)
[![Integration](https://img.shields.io/badge/ATU--100-integrated-green.svg)](https://github.com/frank-ydf/atu-controller)

**Versione del Station Controller con API REST per integrazione con ATU-100 Remote Controller.**

> **ℹ️ Note:** Per la versione standalone senza integrazione ATU, vedi il branch [`main`](https://github.com/frank-ydf/station-controller)

## 🎯 Caratteristiche Aggiuntive

Questo branch estende la versione base con:

- ✅ **API REST** per controllo antenna da remoto
- ✅ **Relay Feedback** - verifica fisica stato relay prima di TX
- ✅ **Integrazione ATU** - switching automatico durante tuning
- ✅ **Backward Compatible** - funziona anche standalone

## 📡 API Endpoints

### GET /api/antenna/status

Ritorna stato antenna HF con verifica relay fisica.

**Response:**
```json
{
  "selected": "590",           // "590", "sdr", o "off"
  "relay": {
    "b1": 1,                   // Relay B1 (GPIO14) - 1=ON, 0=OFF
    "b2": 0,                   // Relay B2 (GPIO26)
    "c1": 0                    // Relay C1 (GPIO25)
  },
  "relay_ok": true,            // true se relay corrisponde a stato logico
  "hf_state": 1,               // 0=off, 1=RTX(590), 2=SDR
  "antenna_state": 1           // 0=off, 1=verticale, 2=longwire
}
```

### POST /api/antenna/590

Switch antenna su TS-590 (HF RTX mode).

**Response:**
```json
{
  "ok": true,
  "selected": "590",
  "relay_ok": true,
  "hf_state": 1
}
```

### POST /api/antenna/sdr

Switch antenna su SDR (HF SDR mode).

**Response:**
```json
{
  "ok": true,
  "selected": "sdr",
  "relay_ok": true,
  "hf_state": 2
}
```

## 🔌 Logica Relay HF

| Mode | HF State | B1 (GPIO14) | B2 (GPIO26) | C1 (GPIO25) | relay_ok |
|------|----------|-------------|-------------|-------------|----------|
| **590 (RTX)** | 1 | ON | OFF | OFF | ✅ |
| **SDR** | 2 | ON | ON | ON | ✅ |
| **OFF** | 0 | OFF | OFF | OFF | ✅ |

Il flag `relay_ok` è `true` solo se lo stato fisico dei relay corrisponde allo stato logico.

## 🚀 Installazione

### Hardware

Identico a versione main:
- ESP32 DevKit
- MCP23017 I2C expander
- 8× Relay module
- Alimentazione 12V

Vedi schema nel branch [`main`](https://github.com/frank-ydf/station-controller)

### Software

```bash
# 1. Clone repository
git clone https://github.com/frank-ydf/station-controller.git
cd station-controller

# 2. Checkout branch integrato
git checkout switch+atu

# 3. Arduino IDE
# - Apri station_master_8rl.ino
# - Board: ESP32 Dev Module
# - Upload

# 4. Serial Monitor (115200 baud)
# Verifica output:
# - "Sistema pronto"
# - "API ATU enabled:"
```

### Configurazione WiFi

Modifica credenziali in `station_master_8rl.ino`:

```cpp
// Linea ~137
WiFi.begin("radio", "radiotest");  // ← modifica SSID e password
```

### Accesso via mDNS

```
http://radio.local          # Interfaccia web
http://radio.local/api/antenna/status   # API status
```

## 🔗 Integrazione con ATU Controller

### Prerequisiti

- Station Controller (questo progetto) configurato e funzionante
- [ATU-100 Remote Controller](https://github.com/frank-ydf/atu-controller) su Raspberry Pi
- Entrambi sulla stessa rete

### Setup ATU Controller

Nel branch [`integrated`](https://github.com/frank-ydf/atu-controller/tree/integrated) del ATU Controller:

1. Modifica `server.js`:
```javascript
// Linea ~12 circa
const STATION_CONTROL_URL = 'http://radio.local';  // ← verifica URL
```

2. Riavvia servizio:
```bash
sudo systemctl restart atu-web
```

### Verifica Integrazione

```bash
# Da Raspberry Pi, testa API
curl http://radio.local/api/antenna/status

# Risposta attesa:
# {"selected":"sdr","relay":{"b1":1,"b2":1,"c1":1},"relay_ok":true,...}

# Test switch
curl -X POST http://radio.local/api/antenna/590
curl http://radio.local/api/antenna/sdr
```

### Flusso Automatico TUNE

Quando premi **TUNE** su ATU Controller:

1. 🔍 Verifica antenna corrente (`GET /api/antenna/status`)
2. 🔀 Se è SDR, switch a 590 (`POST /api/antenna/590`)
3. ⏳ Attende conferma relay (`relay_ok=true`)
4. ✅ Procede con tuning (FSK/RTTY carrier)
5. 🔙 Ripristina antenna originale (se era SDR)

**Totale:** ~25-30 secondi

## 🛠️ Utilizzo

### Modalità Standalone

Funziona esattamente come versione `main`:
- Pulsanti fisici su MCP23017
- Interfaccia web su http://radio.local
- Controllo relay manuale

**Le API non interferiscono con l'uso normale.**

### Modalità Integrata

Quando collegato a ATU Controller:
- Switching automatico antenna durante TUNE
- Verifica relay PRIMA di trasmettere
- Ripristino automatico stato originale
- Log dettagliati su Raspberry Pi

## 📊 Differenze da Branch Main

Vedi comparazione completa:
```
https://github.com/frank-ydf/station-controller/compare/main...switch+atu
```

**Modifiche principali:**
- Aggiunti 3 handler API (`handleAntennaStatus`, `handleSwitch590`, `handleSwitchSDR`)
- Aggiunta verifica fisica relay (`relay_ok` logic)
- Registrati endpoint in `setup()`: `/api/antenna/*`

**Retrocompatibilità:** ✅ 100%
- API esistenti (`/control`, `/getstate`) invariate
- Interfaccia web invariata
- Comportamento pulsanti invariato

## 🐛 Troubleshooting

### API non rispondono

```bash
# Verifica in serial monitor
# Output atteso durante boot:
Sistema pronto
API ATU enabled:
  GET  /api/antenna/status
  POST /api/antenna/590
  POST /api/antenna/sdr
```

### relay_ok sempre false

```bash
# 1. Verifica collegamenti GPIO relay
# B1 = GPIO14, B2 = GPIO26, C1 = GPIO25

# 2. Testa manualmente da web interface
# Switch a 590 → verifica relay fisici

# 3. Serial monitor mostra stato relay:
# Stato rele: A1=X A2=X B1=1 B2=0 C1=0...
```

### mDNS non funziona

```bash
# Usa IP diretto
# 1. Trova IP in serial monitor:
# "Connesso! IP: 192.168.1.XXX"

# 2. Usa IP invece di radio.local:
curl http://192.168.1.XXX/api/antenna/status
```

## 📚 Documentazione Completa

- [INTEGRATION_README.md](INTEGRATION_README.md) - Setup integrazione completa
- [Branch Management Guide](BRANCH_MANAGEMENT.md) - Gestione branch Git

## 🔗 Link Progetti Correlati

- **ATU-100 Remote Controller**: https://github.com/frank-ydf/atu-controller
  - Branch `integrated`: versione con antenna switching
  - Branch `standalone`: versione base
  
- **Station Controller Main**: https://github.com/frank-ydf/station-controller
  - Branch `main`: versione standalone
  - Branch `switch+atu`: questa versione

## 📝 Changelog

### v1.0.0-atu (2024-12-26)
- Prima release branch `switch+atu`
- Aggiunte API REST per integrazione ATU
- Implementata verifica fisica relay
- Documentazione completa

## 📄 License

MIT License - vedi file LICENSE

## 🙏 Crediti

- Station Controller base: IU0AVT/IU1FYF
- Integrazione ATU: IU0AVT/IU1FYF
- ATU-100 firmware originale: [N7DDC](https://github.com/Dfinitski/N7DDC-ATU-100-mini-and-extended-boards)

## 📮 Contatti

- GitHub: [@frank-ydf](https://github.com/frank-ydf)
- Issues: [Station Controller Issues](https://github.com/frank-ydf/station-controller/issues)

---

**73 de IU0AVT** 📻
