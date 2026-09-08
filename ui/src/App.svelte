<script lang="ts">
  import { onMount } from 'svelte';
  type Chord = { label: string; beats: number; startBeat: number; notes: number[] };
  type Pattern = { schema: number; app: string; progression: string; tempo: number; channel: number; velocity: number; octave: number; hostSync: boolean; revision: number; totalBeats: number; chords: Chord[] };
  type Transport = { playing: boolean; armed: boolean; beat: number; tempo: number; activeChord: number; standalone: boolean; hostAvailable: boolean; revision: number };
  let pattern = $state<Pattern>({ schema: 1, app: 'Chord Runner', progression: 'C - G - Am - F', tempo: 120, channel: 1, velocity: 100, octave: 4, hostSync: false, revision: 0, totalBeats: 16,
    chords: [{ label: 'C', beats: 4, startBeat: 0, notes: [60,64,67] }, { label: 'G', beats: 4, startBeat: 4, notes: [67,71,74] }, { label: 'Am', beats: 4, startBeat: 8, notes: [69,72,76] }, { label: 'F', beats: 4, startBeat: 12, notes: [65,69,72] }] });
  let transport = $state<Transport>({ playing: false, armed: false, beat: 0, tempo: 120, activeChord: -1, standalone: false, hostAvailable: false, revision: 0 });
  let draft = $state('C - G - Am - F');
  let error = $state('');
  let connected = $state(false);
  let busy = $state(false);
  let help = $state(false);
  let selected = $state(0);
  let call: (name: string, ...args: unknown[]) => Promise<any> = async () => undefined;
  const dirty = $derived(draft !== pattern.progression);
  const selectedChord = $derived(pattern.chords[Math.min(selected, pattern.chords.length - 1)]);
  const minNote = $derived(Math.max(0, Math.min(...pattern.chords.flatMap(c => c.notes)) - 2));
  const maxNote = $derived(Math.min(127, Math.max(...pattern.chords.flatMap(c => c.notes)) + 2));
  const pitches = $derived(Array.from({ length: maxNote - minNote + 1 }, (_, i) => maxNote - i));
  const noteName = (note: number) => ['C','C♯','D','D♯','E','F','F♯','G','G♯','A','A♯','B'][note % 12] + (Math.floor(note / 12) - 1);

  async function readState() {
    pattern = await call('getState');
    draft = pattern.progression;
    selected = Math.min(selected, pattern.chords.length - 1);
  }
  async function apply(overrides: Partial<Pattern> = {}) {
    if (!connected || busy) return false;
    busy = true;
    try {
      error = await call('setState', { ...pattern, progression: draft, ...overrides });
      if (error) return false;
      await readState();
      return true;
    } catch { error = 'The app connection was interrupted.'; return false; }
    finally { busy = false; }
  }
  async function togglePlay() {
    if (transport.armed) { transport = await call('setPlaying', false); return; }
    if (dirty && !(await apply())) return;
    transport = await call('setPlaying', true);
  }
  async function project(save: boolean) {
    if (save && dirty && !(await apply())) return;
    busy = true;
    try { error = await call(save ? 'saveProject' : 'loadProject'); if (!error) await readState(); }
    catch { error = 'The project operation could not complete.'; }
    finally { busy = false; }
  }
  function keyboard(event: KeyboardEvent) {
    if ((event.metaKey || event.ctrlKey) && event.key === 'Enter') { event.preventDefault(); void apply(); }
    if (event.code === 'Space' && !(event.target instanceof HTMLInputElement || event.target instanceof HTMLTextAreaElement || event.target instanceof HTMLSelectElement || event.target instanceof HTMLButtonElement) && connected) { event.preventDefault(); void togglePlay(); }
  }
  onMount(() => {
    let disposed = false;
    let timer: ReturnType<typeof setTimeout>;
    async function start() {
      if (!window.__JUCE__?.initialisationData?.__juce__functions?.includes('getState')) return;
      const { getNativeFunction } = await import('@juce');
      const functions = new Map<string, (...args: unknown[]) => Promise<any>>();
      call = (name, ...args) => { if (!functions.has(name)) functions.set(name, getNativeFunction(name)); return functions.get(name)!(...args); };
      await readState();
      if (disposed) return;
      connected = true;
      async function poll() {
        try {
          transport = await call('getTransport');
          if (!busy && transport.revision !== pattern.revision) await readState();
        } catch { error = 'The app connection was interrupted.'; }
        if (!disposed) timer = setTimeout(poll, 40);
      }
      void poll();
    }
    void start().catch(() => { error = 'Could not connect to Chord Runner.'; });
    return () => { disposed = true; clearTimeout(timer); };
  });
</script>

<svelte:window onkeydown={keyboard}/>
<div class="app">
  <header>
    <div class="brand"><svg viewBox="0 0 32 32" aria-hidden="true"><defs><linearGradient id="brand-arc-1" x1="0%" y1="0%" x2="100%" y2="100%"><stop offset="0%" stop-color="#06B6D4"/><stop offset="100%" stop-color="#3B82F6"/></linearGradient><linearGradient id="brand-arc-2" x1="0%" y1="0%" x2="100%" y2="100%"><stop offset="0%" stop-color="#6366F1"/><stop offset="100%" stop-color="#A855F7"/></linearGradient><linearGradient id="brand-arc-3" x1="0%" y1="0%" x2="100%" y2="100%"><stop offset="0%" stop-color="#D946EF"/><stop offset="100%" stop-color="#F43F5E"/></linearGradient><linearGradient id="brand-arc-4" x1="0%" y1="0%" x2="100%" y2="100%"><stop offset="0%" stop-color="#FB923C"/><stop offset="100%" stop-color="#FBBF24"/></linearGradient></defs><rect width="32" height="32" rx="7" fill="#09090b"/><g transform="rotate(-22 16 16)"><ellipse cx="16" cy="16" rx="10" ry="6.5" fill="none" stroke="#1c192c" stroke-width="2.8"/><path d="M 16,9.5 A 10 6.5 0 0 1 25,14" fill="none" stroke="url(#brand-arc-1)" stroke-width="2.4" stroke-linecap="round"/><path d="M 24.5,17 A 10 6.5 0 0 1 17,22.5" fill="none" stroke="url(#brand-arc-2)" stroke-width="2.4" stroke-linecap="round"/><path d="M 15,22.5 A 10 6.5 0 0 1 7,18" fill="none" stroke="url(#brand-arc-3)" stroke-width="2.4" stroke-linecap="round"/><path d="M 7.5,14.5 A 10 6.5 0 0 1 15,9.6" fill="none" stroke="url(#brand-arc-4)" stroke-width="2.4" stroke-linecap="round"/></g><circle cx="23" cy="19.5" r="1.6" fill="#FFFFFF"/><circle cx="23" cy="19.5" r="3" fill="#38BDF8" opacity="0.5"/></svg><h1>Chord Runner</h1><span class="byline">ofsound</span></div>
    <nav aria-label="Project"><button disabled={!connected || busy} onclick={() => project(false)}>Open</button><button disabled={!connected || busy} onclick={() => project(true)}>Save</button><button class:chosen={help} onclick={() => help = !help} aria-expanded={help}>Syntax <span class="question">?</span></button></nav>
  </header>

  <main>
    <section class="composer" aria-labelledby="progression-label">
      <div class="section-label"><label id="progression-label" for="progression">CHORD PROGRESSION</label><span>{dirty ? 'Unapplied changes' : 'One chord. One bar. Keep going.'}</span></div>
      <div class="input-row"><input id="progression" spellcheck="false" autocomplete="off" maxlength="4096" bind:value={draft} placeholder="C - G - Am - F" aria-describedby="syntax-hint"/><button class="apply" disabled={!connected || busy} onclick={() => apply()}>Apply <span>↵</span></button></div>
      <div class="input-caption" id="syntax-hint"><span>Try <button class="text-button" onclick={() => draft = 'C1/2 G1/2 | Am2'}>C1/2 G1/2 | Am2</button> <span class="muted">— durations in bars</span></span><span class="shortcut">⌘ / Ctrl + Enter</span></div>
      {#if error}<p class="error" role="alert">{error}</p>{/if}
      {#if help}<div class="help"><p><b>Roots</b> C, D, E, F, G, A, B · sharps # · flats b</p><p><b>Chords</b> C · Cm · C7 · Cmaj7 · Cm7 · Cdim · Cdim7 · Cm7b5 · Caug · Csus2 · Csus4 · C5</p><p><b>Time</b> One bar by default, in 4/4. C1/2 = half a bar; Am2 = two bars. Spaces, dashes and | separate chords. The | separator does not add time.</p><p><b>Explicit duration</b> Use @ to avoid ambiguity: C7@2 = C dominant seventh for two bars. No inversions yet.</p></div>{/if}
    </section>

    <section class="transport" aria-label="Transport and MIDI settings">
      <button class="play" class:running={transport.armed} disabled={!connected || busy} onclick={togglePlay}><span>{transport.armed ? '■' : '▶'}</span>{transport.armed ? 'Stop' : pattern.hostSync && !transport.standalone ? 'Arm' : 'Play'}</button>
      <label class="control">TEMPO <div><input aria-label="Tempo" type="number" min="20" max="300" step="1" value={pattern.tempo} disabled={!connected || busy || (pattern.hostSync && !transport.standalone)} onchange={e => apply({tempo: Number(e.currentTarget.value)})}/><span>BPM</span></div></label>
      <label class="control">CLOCK<select aria-label="Clock" value={pattern.hostSync && !transport.standalone ? 'host' : 'internal'} disabled={!connected || busy || transport.standalone} onchange={e => apply({hostSync: e.currentTarget.value === 'host'})}><option value="internal">Internal</option><option value="host">Host sync</option></select></label>
      <div class="transport-spacer"></div>
      <label class="control">OCTAVE<select aria-label="Octave" value={pattern.octave} disabled={!connected || busy} onchange={e => apply({octave: Number(e.currentTarget.value)})}>{#each [0,1,2,3,4,5,6,7,8] as octave}<option value={octave}>{octave}</option>{/each}</select></label>
      <label class="control">VELOCITY<input aria-label="Velocity" type="number" min="1" max="127" value={pattern.velocity} disabled={!connected || busy} onchange={e => apply({velocity: Number(e.currentTarget.value)})}/></label>
      <label class="control">CHANNEL<select aria-label="MIDI channel" value={pattern.channel} disabled={!connected || busy} onchange={e => apply({channel: Number(e.currentTarget.value)})}>{#each Array.from({length: 16}, (_, i) => i + 1) as channel}<option value={channel}>{channel}</option>{/each}</select></label>
    </section>

    <section class="sequence" aria-label="Chord timeline">
      <div class="section-label"><span>TIMELINE</span><span>{pattern.chords.length} chords <i>·</i> {Number((pattern.totalBeats / 4).toFixed(3))} bars <i>·</i> 4/4 <i>·</i> Loop</span></div>
      <div class="timeline-scroll"><div class="timeline" style:min-width={`${Math.max(600, pattern.chords.length * 100)}px`}>
        <div class="ruler"><div class="key-label">BAR</div><div class="bar-numbers">{#each Array.from({length: Math.ceil(pattern.totalBeats / 4)}, (_, i) => i) as bar}<span style:left={`${bar * 4 / pattern.totalBeats * 100}%`}>{bar + 1}</span>{/each}</div></div>
        <div class="chord-strip"><div class="key-label">CHORD</div><div class="chords">{#each pattern.chords as chord, i}<button class:selected={selected === i} class:active={transport.activeChord === i} style:flex={chord.beats} onclick={() => selected = i} aria-label={`${chord.label}, ${chord.beats / 4} bars, MIDI ${chord.notes.join(', ')}`}><b>{chord.label}</b><span>{Number((chord.beats / 4).toFixed(3))} {chord.beats === 4 ? 'bar' : 'bars'}</span></button>{/each}</div></div>
        <div class="piano-roll"><div class="pitch-labels">{#each pitches as pitch}<div class:black={[1,3,6,8,10].includes(pitch % 12)}>{noteName(pitch)}</div>{/each}</div><div class="roll-grid">
          {#each pitches as pitch}<div class="pitch-row" class:black={[1,3,6,8,10].includes(pitch % 12)}></div>{/each}
          {#each pattern.chords as chord, i}{#each chord.notes as note}<div class="note" class:sounding={transport.activeChord === i} style:left={`${chord.startBeat / pattern.totalBeats * 100}%`} style:width={`calc(${chord.beats / pattern.totalBeats * 100}% - 3px)`} style:top={`calc(${(maxNote - note) / pitches.length * 100}% + 2px)`} style:height={`calc(${100 / pitches.length}% - 4px)`}><span>{noteName(note)}</span></div>{/each}{/each}
          {#each Array.from({length: Math.ceil(pattern.totalBeats / 4)}, (_, i) => i) as bar}<div class="bar-line" style:left={`${bar * 4 / pattern.totalBeats * 100}%`}></div>{/each}
          {#if transport.playing}<div class="playhead" style:left={`${transport.beat / pattern.totalBeats * 100}%`}></div>{/if}
        </div></div>
      </div></div>
      <div class="note-inspector"><div><b>{selectedChord.label}</b><span>{selectedChord.notes.map(noteName).join(' · ')}</span></div><span class="midi-values">MIDI <b>{selectedChord.notes.join(' · ')}</b></span></div>
    </section>
  </main>
  <footer><div><span class="status-dot" class:lit={connected}></span>{!connected ? 'Browser preview · open the app to send MIDI' : transport.standalone ? 'Chord Runner Output' : 'MIDI → host'}<i>·</i><span>{transport.playing ? `Playing at ${Number(transport.tempo.toFixed(1))} BPM` : transport.armed && pattern.hostSync ? transport.hostAvailable ? 'Waiting for host transport' : 'Host transport unavailable' : 'Stopped'}</span></div><button disabled={!connected} onclick={async () => { await call('panic'); transport = await call('getTransport'); }}>All notes off</button></footer>
</div>
