# blf_02
Mysql UDF extension to handle bloom filter checking in the database

This code implements 2 functions to be added to MySQL via the UDF framework.  

License: Apache

Bloomfilters are assumed to be arrays of bytes in little-endian order.  

# Installation
Install package as normal.
```sh
yum install blf_02.rpm
```

## Enabling

Read more for permissions required: https://mariadb.com/kb/en/user-defined-functions-security/

### Option 1 - Execute the premade query
```
mariadb < /opt/teragrep/blf_02/share/installdb.sql
```

### Option 2 - Execute the queries manually

```
USE mysql;

DROP FUNCTION IF EXISTS bloommatch;
DROP FUNCTION IF EXISTS bloomupdate;
CREATE FUNCTION bloommatch RETURNS integer SONAME 'lib_mysqludf_bloom.so';
CREATE FUNCTION bloomupdate RETURNS STRING SONAME 'lib_mysqludf_bloom.so';
```

## Disabling

Read more for permissions required: https://mariadb.com/kb/en/user-defined-functions-security/

### Option 1 - Execute the premade query
```
mariadb < /opt/teragrep/blf_02/share/uninstalldb.sql
```

### Option 2 - Execute the queries manually

```
USE mysql;

DROP FUNCTION IF EXISTS bloommatch;
DROP FUNCTION IF EXISTS bloomupdate;
```

# Functions

```
bloommatch( blob a, blob b )
```
performs a byte by bytes check of  (a & b == a).  if true then "a" may be found in "b", if false then "a" is not in "b".
example:

```
Connection con = ... // get the db connection
InputStream is = ... // input stream containing a the bloom filter to locate in the table
PreparedStatement stmt = con.prepareStatement( "SELECT * FROM bloomTable WHERE bloommatch( ?, bloomTable.filter );" );
stmt.setBlob( 1, is );
ResultSet rs = stmt.executeQuery();
// rs now contains all the matching bloom filters from the table.
```

```
bloomupdate( blob a, blob b )
```
performs a byte by byte construct of a new filter where (a | b). 
example:

```
Connection con = ... // get the db connection
InputStream is = ... // input stream containing a the bloom filter to locate in the table
PreparedStatement stmt = con.prepareStatement( "UPDATE bloomTable SET filter=bloomupdate( ?, bloomTable.filter ) WHERE id=?;" );
stmt.setBlob( 1, is );
stmt.setint( 2, 5 );
stmt.executeUpdate();
// bloom filters on rows with id of 5 have been updated to include values from the blob.
```

# Development

Mysql client and server headers are required to compile this code.

Please do the following in the root of the source directory tree:
```sh
aclocal
autoconf
autoheader
automake --add-missing

./configure
make
sudo make install
sudo make installdb
```

To remove the library from your system:

```
make uninstalldb
make uninstall
```

# Examples

## Spark

Short demo how to use in practice using spark and scala.

Step 1. Creating and storing filter to database:
```
%spark

// Generate and upload a spark bloomfilter to database

import spark.implicits._
import org.apache.spark.sql._
import org.apache.spark.sql.types._
import java.sql.DriverManager
import org.apache.spark.util.sketch.BloomFilter
import java.io.{ByteArrayOutputStream,ByteArrayInputStream, ObjectOutputStream, InputStream}

// Filter parameters
val expected: Long = 500
val fpp: Double = 0.3

val dburl = "DATABASE_URL"
val updatesql = "INSERT token_partitions (`partition`, `filter`) VALUES (?,?)"
val conn = DriverManager.getConnection(dburl,"DB_USERNAME","DB_PASSWORD")

// Create a spark Dataframe with values 'one','two' and 'three'
val in1 = spark.sparkContext.parallelize(List("one","two","three"))
val df = in1.toDF("tokens")

val ps = conn.prepareStatement(updatesql)

// Create a bloomfilter from the Dataframe
val filter = df.stat.bloomFilter($"tokens", expected, fpp)
println(filter.mightContain("one"))

// Write filter bit array to output stream
val baos = new ByteArrayOutputStream
filter.writeTo(baos)
val is: InputStream = new ByteArrayInputStream(baos.toByteArray())
ps.setString(1,"1")
ps.setBlob(2,is)
val update = ps.executeUpdate
println("Updated rows: "+ update)
df.show()
conn.close()
```

Step 2. Finding matching filters:
```
%spark

// Create a bloomfilter and find matches
import spark.implicits._
import org.apache.spark.sql._
import org.apache.spark.sql.types._
import java.sql.DriverManager
import org.apache.spark.util.sketch.BloomFilter
import java.io.{ByteArrayOutputStream,ByteArrayInputStream, ObjectOutputStream, InputStream}

val expected: Long = 500
val fpp: Double = 0.3

val dburl = "DATABASE_URL"
val conn = DriverManager.getConnection(dburl,"DB_USERNAME","DB_PASSWORD")

val updatesql = "SELECT `partition` FROM token_partitions WHERE bloommatch(?, token_partitions.filter);"
val ps = conn.prepareStatement(updatesql)

// Creating filter with values 'one' and 'two'
val in2 = spark.sparkContext.parallelize(List("one","two"))
val df2 = in2.toDF("tokens")
val filter = df2.stat.bloomFilter($"tokens", expected, fpp)

val baos = new ByteArrayOutputStream
            filter.writeTo(baos)
            baos.flush
            val is :InputStream = new ByteArrayInputStream(baos.toByteArray())
            ps.setBlob(1, is)
            val rs = ps.executeQuery

// Will find a match since tokens searched are a subset of the database filter
val resultList = Iterator.from(0).takeWhile(_ => rs.next()).map(_ => rs.getString(1)).toList
println("Found matches: " + resultList.size)
conn.close()
```

SQL table used in demo.
```
CREATE TABLE `token_partitions` (
`id` INT unsigned NOT NULL auto_increment,
`partition` VARCHAR(100),
`filter` BLOB,
PRIMARY KEY (`id`)
);
```