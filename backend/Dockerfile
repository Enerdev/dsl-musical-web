FROM node:20-slim

RUN apt-get update && apt-get install -y g++ && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copiamos todo: server.js, package.json y los .h/.cpp del compilador
COPY . .

RUN npm install --production

# Compila el compilador C++ (main_web.cpp lee stdin, ya no archivos)
RUN g++ -O2 -std=c++17 -o compilador main_web.cpp

EXPOSE 3000
CMD ["node", "server.js"]
