try:
    from lib import install_text
except ImportError:
    pass

CONTENT = {
"title": "DRIFT STATION", "file": "DriftStation", "repo": "DriftStation",
"accent": "#4fd1e0", "accent2": "#8a6cf0",
"en": {
 "subtitle": "Drifting memory processor",
 "kind": "Audio effect for macOS (Apple Silicon)  \xb7  VST3, Audio Unit, CLAP and Standalone",
 "by": "by Dani Koenig",
 "intro": ("Drift Station records whatever you feed it into a large sound memory and keeps recombining it. The delay lines have different prime-number lengths, "
           "an asynchronous LFO moves them and a slow chaotic signal modulates that LFO. The material layers up endlessly, like a looper, but without an obvious repeating loop. "
           "Made for ambient, drones and evolving soundscapes."),
 "note": "Inspired by the SOMA Laboratory COSMOS pedal. Drift Station is an independent project and not affiliated with SOMA Laboratory.",
 "blocks": [("Algorithms (each in 3 sizes)", [
   ("2 Delays", "S about 2.5 s, M about 9.5 s, L about 22 s."),
   ("4 Delays", "S about 2.5 s, M about 8.5 s, L about 11.5 s."),
   ("Giant Reverb", "Huge hall, super-huge hall, insanely-super-huge hall."),
   ("Granular", "Small, medium and large grains. L adds a centre grain delay."),
 ])],
 "quick_title": "Quick start",
 "quick": [
   ("1. Insert", "Put Drift Station on a track with a drone, pad or any sustained sound. Set <b>Mix</b> to the centre."),
   ("2. Record", "<b>Record</b> = All. The memory fills with what you play."),
   ("3. Grow", "Raise <b>Feedback</b> slowly towards 1.0. The sound builds up and drifts."),
   ("4. Play over it", "Switch <b>Record</b> to Off to freeze the memory and play live over it."),
 ],
 "first": "All algorithms share one memory. When you switch, the content stays and is processed by the new algorithm.",
 "sections": [
  ("#4fd1e0", "INPUT AND MEMORY", [
    ("IN (Auto)", "Auto gain on the input. Every source lands at a good working level. The knob trims the target by plus or minus 12 dB."),
    ("GATE", "Noise gate on the input."),
    ("MONO IN", "Sums the input to mono."),
    ("RECORD", "Off, Group 1 (left input), Group 2 (right input) or All."),
    ("REVERSE / HALF SPEED", "Reverse and half speed for the delays."),
    ("ERASE (hold) / CLEAR ALL", "Erase fades the memory while you hold it. Clear All empties it."),
  ]),
  ("#8a6cf0", "SHAPE", [
    ("BLUR", "Delays: cross-feedback between the lines. Granular: noise on the grain position."),
    ("DRIFT", "Delays: a chaotic LFO moves the lines in the stereo field, each line stays on its side so the pitch stays steady. Granular: moves the grain positions."),
    ("TRANSPOSE", "Transposes the memory output. Centre = 0, left down to -24, right up to +12 semitones."),
    ("SUP | COM", "Left = suppressor (new sounds push out the recorded ones). Right = compressor."),
    ("FEEDBACK", "0 to 1.25, the white tick is 1.0. A soft tape-style limiter prevents hard clipping."),
    ("HPF / LPF", "Filters inside the loop, 3 settings each."),
  ]),
  ("#e0a800", "OUTPUT", [
    ("LIMITER", "Output limiter with auto gain, ceiling -1 dBFS. Fully open only catches peaks. Turn it back to lower the threshold, the ring lights red with the gain reduction."),
    ("MIX (far right)", "Input versus memory, centre = equal. Turn to the centre to play live over your drone."),
    ("Stereo sway", "The memory output moves slowly between left and right (periods of 83 s and 131 s), like the hardware."),
  ]),
  ("#ff7a2f", "WINDOW", [
    ("Size", "Opens at 100 %. Drag the bottom right corner to resize, 70 % to 200 %."),
  ]),
 ],
 "recipes": [
   ("Endless drone", "Algorithm 4 Delays L. Play a chord, then Record Off. Feedback 1.0, Drift 40 %, Mix at the centre."),
   ("Cloud", "Granular M, Blur 50 %, Transpose +12. Feed it a piano or a voice."),
   ("Dark space", "Giant Reverb, LPF on, Feedback 0.9. Good on single notes and short hits."),
   ("Play over a drone", "Record a drone for 20 s, Record Off, then play your synth with Mix at the centre."),
 ],
 "tips": [
   "Feedback above 1.0 keeps the sound growing. Watch the Limiter ring.",
   "Erase fades the memory gently, Clear All is instant.",
   "Everything can be automated. Memory content is not saved with the project.",
   "Drift Station also runs as a standalone app.",
 ],
 "install": "",
},
"de": {
 "subtitle": "Driftender Memory-Prozessor",
 "kind": "Audio-Effekt für macOS (Apple Silicon)  \xb7  VST3, Audio Unit, CLAP und Standalone",
 "by": "von Dani Koenig",
 "intro": ("Drift Station nimmt alles, was du hineinschickst, in einen grossen Klangspeicher auf und mischt es laufend neu. Die Delay-Linien haben unterschiedliche Primzahl-Längen, "
           "ein asynchroner LFO bewegt sie und ein langsames chaotisches Signal moduliert diesen LFO. Das Material schichtet sich endlos wie bei einem Looper, aber ohne erkennbaren Loop. "
           "Gemacht für Ambient, Drones und sich entwickelnde Klanglandschaften."),
 "note": "Inspiriert vom SOMA Laboratory COSMOS. Drift Station ist ein unabhängiges Projekt und nicht mit SOMA Laboratory verbunden.",
 "blocks": [("Algorithmen (je in 3 Grössen)", [
   ("2 Delays", "S ca. 2,5 s, M ca. 9,5 s, L ca. 22 s."),
   ("4 Delays", "S ca. 2,5 s, M ca. 8,5 s, L ca. 11,5 s."),
   ("Giant Reverb", "Riesiger Hall, super-riesiger Hall, wahnsinnig-super-riesiger Hall."),
   ("Granular", "Kleine, mittlere und grosse Grains. L hat zusätzlich ein Grain-Delay in der Mitte."),
 ])],
 "quick_title": "Schnellstart",
 "quick": [
   ("1. Einfügen", "Drift Station auf eine Spur mit Drone, Pad oder einem anderen Dauerton legen. <b>Mix</b> auf die Mitte."),
   ("2. Aufnehmen", "<b>Record</b> = All. Der Speicher füllt sich mit dem, was du spielst."),
   ("3. Wachsen lassen", "<b>Feedback</b> langsam Richtung 1.0 erhöhen. Der Klang baut sich auf und driftet."),
   ("4. Darüber spielen", "<b>Record</b> auf Off stellen: Der Speicher wird eingefroren, du spielst live darüber."),
 ],
 "first": "Alle Algorithmen teilen einen Speicher. Beim Wechseln bleibt der Inhalt erhalten und wird vom neuen Algorithmus verarbeitet.",
 "sections": [
  ("#4fd1e0", "EINGANG UND SPEICHER", [
    ("IN (Auto)", "Auto-Gain am Eingang. Jede Quelle landet auf einem guten Arbeitspegel. Der Regler verschiebt das Ziel um plus oder minus 12 dB."),
    ("GATE", "Noise-Gate am Eingang."),
    ("MONO IN", "Summiert den Eingang zu Mono."),
    ("RECORD", "Off, Group 1 (linker Eingang), Group 2 (rechter Eingang) oder All."),
    ("REVERSE / HALF SPEED", "Rückwärts und halbe Geschwindigkeit für die Delays."),
    ("ERASE (halten) / CLEAR ALL", "Erase blendet den Speicher aus, solange du hältst. Clear All leert ihn."),
  ]),
  ("#8a6cf0", "FORM", [
    ("BLUR", "Delays: Cross-Feedback zwischen den Linien. Granular: Rauschen auf der Grain-Position."),
    ("DRIFT", "Delays: Ein chaotischer LFO bewegt die Linien im Stereobild, jede Linie bleibt auf ihrer Seite, damit die Tonhöhe stabil bleibt. Granular: bewegt die Grain-Positionen."),
    ("TRANSPOSE", "Transponiert den Speicher-Ausgang. Mitte = 0, links bis -24, rechts bis +12 Halbtöne."),
    ("SUP | COM", "Links = Suppressor (neue Klänge verdrängen die aufgenommenen). Rechts = Kompressor."),
    ("FEEDBACK", "0 bis 1,25, der weisse Strich ist 1,0. Ein weicher Tape-Limiter verhindert hartes Clipping."),
    ("HPF / LPF", "Filter in der Schleife, je 3 Einstellungen."),
  ]),
  ("#e0a800", "AUSGANG", [
    ("LIMITER", "Ausgangs-Limiter mit Auto-Gain, Ceiling -1 dBFS. Ganz offen fängt er nur Spitzen. Zurückdrehen senkt den Threshold, der Ring leuchtet rot mit der Gain Reduction."),
    ("MIX (ganz rechts)", "Eingang gegen Speicher, Mitte = gleich. Auf die Mitte drehen, um live über deinen Drone zu spielen."),
    ("Stereo-Sway", "Der Speicher-Ausgang wandert langsam zwischen links und rechts (Perioden von 83 s und 131 s), wie bei der Hardware."),
  ]),
  ("#ff7a2f", "FENSTER", [
    ("Grösse", "Öffnet bei 100 %. Ecke unten rechts ziehen zum Skalieren, 70 % bis 200 %."),
  ]),
 ],
 "recipes": [
   ("Endloser Drone", "Algorithmus 4 Delays L. Akkord spielen, dann Record Off. Feedback 1.0, Drift 40 %, Mix in der Mitte."),
   ("Wolke", "Granular M, Blur 50 %, Transpose +12. Klavier oder Stimme hineinschicken."),
   ("Dunkler Raum", "Giant Reverb, LPF an, Feedback 0,9. Gut für einzelne Töne und kurze Hits."),
   ("Über Drone spielen", "20 s Drone aufnehmen, Record Off, dann den Synth spielen, Mix in der Mitte."),
 ],
 "tips": [
   "Feedback über 1,0 lässt den Klang weiter anwachsen. Achte auf den Limiter-Ring.",
   "Erase blendet den Speicher sanft aus, Clear All sofort.",
   "Alles lässt sich automatisieren. Der Speicherinhalt wird nicht mit dem Projekt gespeichert.",
   "Drift Station läuft auch als Standalone-App.",
 ],
 "install": "",
},
}
for _l in ("en", "de"):
    CONTENT[_l]["install"] = install_text(_l, "DriftStation", "DriftStation", "Drift Station")
