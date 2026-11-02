// Emscripten's CLI helper allocates argv on the stack without restoring it.
// Keep repeated calls independent and leave the caller's argument array intact.
const invokeMain = Module['callMain'];
Module['callMain'] = (args = []) => {
  const stack = Module['stackSave']();
  try {
    return invokeMain([...args]);
  } finally {
    Module['stackRestore'](stack);
  }
};
