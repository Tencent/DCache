const Tars = require("@tars/rpc");
const QueryEndpoint = require("./QueryFProxy").tars

function EndpointManager(locator) {
    this.locator = locator;
    this.QueryEndpointPrx = Tars.client.stringToProxy(QueryEndpoint.QueryFProxy, this.locator);
}

EndpointManager.prototype.getQueryPrx = function () {
    return this.QueryEndpointPrx;
}

EndpointManager.prototype.getActiveEndpointFromLocator = async function (obj) {
    const self = this;
    let Res = await self.QueryEndpointPrx.findObjectById(obj).catch(err => {
        throw err;
    });
    let endArr = Res.response.return.value;
    let ObjList = [];
    for (let v of endArr) {
        ObjList.push(`${obj}@${v.istcp?"tcp":"udp"} -h ${v.host} -p ${v.port}`);
    }
    return ObjList;
}
module.exports = EndpointManager;