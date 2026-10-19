
FROM gcc:13

WORKDIR /app

COPY include/ ./include/
COPY public/ ./public/
COPY server.cpp .

RUN g++ -std=c++17 -O2 -I include server.cpp -o server -pthread

EXPOSE 10000

CMD ["sh", "-c", "./server"]
