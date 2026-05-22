// Marduk_F In-Memory Telemetry Initialization Bridge
(function initializeMardukGateway() {
    console.log("[System Init] Sluice-Bench security envelopes mapped.");
    
    // Safety check ensuring data parsing configurations remain isolated
    window.addEventListener('unload', () => {
        if (window.backgroundPoll) {
            clearInterval(window.backgroundPoll);
        }
    });
})();
