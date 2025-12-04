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
            'Please reinstall: npm install @superdev22/pgnx or rebuild: npm run build'
        );
    }
}
