declare module '@juce' {
  export function getNativeFunction(name: string): (...args: unknown[]) => Promise<any>;
}
interface Window {
  __JUCE__?: { initialisationData?: { __juce__functions?: string[] } };
}
