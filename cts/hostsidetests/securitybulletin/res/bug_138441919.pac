function FindProxyForURL(url, host){
    Object.defineProperty(Promise, Symbol.species, { value: 0 });
    var p = new Promise(function() {});
    p.then();
    return "DIRECT";
}