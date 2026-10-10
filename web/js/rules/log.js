// The C++ rules print to std::cout. Here every message becomes an event with
// the original text plus structured data, so the battle scene can animate it.
let sink = null;

export function setSink(fn) {
  const previous = sink;
  sink = fn;
  return previous;
}

export function emit(type, text = '', data = {}) {
  if (sink) sink({ type, text, ...data });
}

// Runs fn while collecting every event it emits; restores the previous sink.
export function collect(fn) {
  const events = [];
  const previous = setSink((event) => {
    events.push(event);
    if (previous) previous(event);
  });
  try {
    const result = fn();
    return { result, events };
  } finally {
    setSink(previous);
  }
}
