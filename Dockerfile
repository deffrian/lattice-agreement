# syntax=docker/dockerfile:1

FROM rikorose/gcc-cmake

COPY . ./

WORKDIR /build

RUN cmake ..

RUN make zheng

CMD ["./zheng/zheng $PORT $ID $ELEM > log.txt"]
