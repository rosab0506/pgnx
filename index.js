const load = require('node-gyp-build');
const native = load(__dirname);

// Export the Connection class from the native module
module.exports = { Connection: native.Connection };
