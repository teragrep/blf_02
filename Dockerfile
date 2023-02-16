FROM centos:7
RUN yum -y install make automake autoconf libtool mariadb-devel
WORKDIR /code
COPY docker-entrypoint.sh /docker-entrypoint.sh
ENTRYPOINT [ "/docker-entrypoint.sh" ]
