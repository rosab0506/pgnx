const path = require('path');

try {
    // Try to load prebuilt binary first (for npm install)
    const { Connection } = require('node-gyp-build')(path.join(__dirname));
    module.exports = { Connection };
} catch (error) {
    // Fallback to build directory (for development)
    try {
        const { Connection } = require(path.join(__dirname, 'build', 'Release', 'pgnx.node'));
        module.exports = { Connection };
    } catch (buildError) {
        throw new Error(
            'Failed to load pgnx native addon. ' +
            'Make sure prebuilt binaries are available for your platform, ' +
            'or rebuild from source: npm run build\n' +
            'Error: ' + (error.message || buildError.message)
        );
    }
}
