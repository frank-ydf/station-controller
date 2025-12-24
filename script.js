class StationController {
  constructor() {
    this.currentState = { antenna: 0, hf: 0, vuhf: 0 };
    this.syncInterval = null;
    this.init();
  }

  async init() {
    this.setupEventListeners();
    this.updateClock();
    await this.fetchInitialState();
    setInterval(() => this.updateClock(), 1000);
    this.startSync();
    
    // Remove loading screen with fade out
    setTimeout(() => {
      const loading = document.getElementById('loading');
      if (loading) {
        loading.style.transition = 'opacity 0.5s ease';
        loading.style.opacity = '0';
        setTimeout(() => loading.remove(), 500);
      }
    }, 1000);
  }

  startSync() {
    this.syncInterval = setInterval(() => this.syncState(), 2000);
  }

  async fetchInitialState() {
    try {
      const response = await fetch('/getstate');
      if (response.ok) {
        this.currentState = await response.json();
        this.updateUI();
      }
    } catch (e) {
      console.error("Initial state error:", e);
    }
  }

  async syncState() {
    try {
      const response = await fetch('/getstate');
      if (response.ok) {
        const newState = await response.json();
        if (JSON.stringify(newState) !== JSON.stringify(this.currentState)) {
          this.currentState = newState;
          this.updateUI();
        }
      }
    } catch (e) {
      console.log("Sync error:", e);
    }
  }

  updateClock() {
    const now = new Date();
    const utcTime = now.toUTCString().split(' ')[4];
    const localTime = now.toLocaleTimeString('it-IT', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    
    document.getElementById('utc-time').textContent = utcTime;
    document.getElementById('local-time').textContent = localTime;
  }

  async sendCommand(cmd, val) {
    try {
      const response = await fetch('/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `cmd=${cmd}&val=${val}`
      });
      
      if (response.ok) {
        this.currentState = await response.json();
        this.updateUI();
      }
    } catch (error) {
      console.error('Command failed:', error);
    }
  }

  async sendMultipleCommands(commands) {
    // Send multiple commands in sequence
    for (const {cmd, val} of commands) {
      await this.sendCommand(cmd, val);
    }
  }

  updateUI() {
    // Update all matrix switches based on current state
    document.querySelectorAll('.matrix-switch').forEach(cell => {
      cell.classList.remove('active');
      
      if (cell.classList.contains('disabled')) return;
      
      const radioCmd = cell.dataset.radio;
      const radioVal = parseInt(cell.dataset.radioVal);
      const antennaCmd = cell.dataset.antenna;
      const antennaVal = parseInt(cell.dataset.antennaVal);
      
      // Check if this cell represents the current active connection
      const radioActive = (radioCmd === 'hf' && this.currentState.hf === radioVal) ||
                          (radioCmd === 'vuhf' && this.currentState.vuhf === radioVal);
      
      const antennaActive = (antennaCmd === 'antenna' && this.currentState.antenna === antennaVal) ||
                            (antennaCmd === 'vuhf' && this.currentState.vuhf === antennaVal);
      
      if (radioActive && antennaActive) {
        cell.classList.add('active');
      }
    });
  }

  setupEventListeners() {
    // Matrix switch click handlers
    document.querySelectorAll('.matrix-switch').forEach(cell => {
      if (!cell.classList.contains('disabled')) {
        cell.addEventListener('click', async () => {
          const radioCmd = cell.dataset.radio;
          const radioVal = parseInt(cell.dataset.radioVal);
          const antennaCmd = cell.dataset.antenna;
          const antennaVal = parseInt(cell.dataset.antennaVal);
          
          // Send both radio and antenna commands
          await this.sendCommand(radioCmd, radioVal);
          
          // Only send antenna command if it's different from the radio command
          // (for V/UHF the antenna command uses the same vuhf parameter)
          if (antennaCmd !== radioCmd) {
            await this.sendCommand(antennaCmd, antennaVal);
          }
        });
      }
    });
    
    // Master off button
    document.getElementById('master-off').addEventListener('click', () => {
      if (confirm('Sei sicuro di voler disattivare tutti i sistemi?')) {
        this.sendCommand('master_off', 0);
      }
    });
  }
}

// Initialize controller when DOM is ready
new StationController();
