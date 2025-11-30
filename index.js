const path = require('path');

try {
    const { Connection } = require(path.join(__dirname, 'build', 'Release', 'pgnx.node'));
    module.exports = { Connection };
} catch (error) {
    throw new Error(
        'Failed to load pgnx native addon. ' +
        'Please reinstall: npm install pgnx'
    );
}
