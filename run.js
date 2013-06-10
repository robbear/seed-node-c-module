var module = require('./build/Release/seedmodule');

module.async(function(err, result) {
    console.log(result);
});
