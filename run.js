var module = require('./build/Release/seedmodule');
var sleepTime = 3;

console.log("Calling module.async to sleep for 3 seconds...");
module.async(sleepTime * 1000, function(err, result) {
    console.log("Finished sleeping for %d milliseconds", result);
});
console.log("Note that we're not blocking!");
console.log("We can do whatever we want for %d seconds", sleepTime);
