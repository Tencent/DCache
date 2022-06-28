FROM tarscloud/base-compiler 

RUN mkdir -p /data
COPY . /data
RUN cd /data \
    && mkdir -p build   \
    && cd build    \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j4

