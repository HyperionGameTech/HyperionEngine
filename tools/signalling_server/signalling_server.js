const http      = require('http');
const websocket = require('websocket');

const clients = new Map();

const httpServer = http.createServer((req, res) => {
    console.log(`${req.method.toUpperCase()} ${req.url}`);

    const respond = (code, data, contentType = 'text/plain') => {
        res.writeHead(code, {
            'Content-Type': contentType,
            'Access-Control-Allow-Origin': '*',
        });
        res.end(data);
    };

    respond(404, 'Not Found');
});

const wsServer = new websocket.server({ httpServer });

wsServer.on('request', (req) => {
    const { path } = req.resourceURL;
    const splitted = path.split('/');
    splitted.shift();
    const id = splitted[0];

    let testInterval = null;

    const conn = req.accept(null, req.origin);

    testInterval = setInterval(() => {
        console.log(`Client ${id} >> Ping`)
        conn.send(JSON.stringify({
            type: 'Ping',
            timestamp: Date.now()
        }));
    }, 1000);

    conn.on('message', (data) => {
        let str = '';

        if (data.type === 'utf8') {
            str = data.utf8Data;
        } else if (data.type === 'binary') {
            str = data.binaryData.toString('utf8');
        }

        console.log(`Client ${id} << ${str}`);

        const message = JSON.parse(str);
        const destId = message.id;
        const dest = clients.get(destId);

        if (dest) {
            message.id = id;
            const data = JSON.stringify(message);
            console.log(`Client ${destId} >> ${data}`);
            dest.send(data);
        } else {
            console.error(`Client ${destId} not found`);
        }
    });

    conn.on('close', () => {
        if (testInterval) {
            clearInterval(testInterval);
        }

        clients.delete(id);
        console.error(`Client ${id} disconnected`);
    });

    clients.set(id, conn);
});

const endpoint = process.env.PORT || '8000';
const splitted = endpoint.split(':');
const port = splitted.pop();
const hostname = splitted.join(':') || '127.0.0.1';

httpServer.listen(port, hostname,
    () => { console.log(`Server listening on ${hostname}:${port}`); });