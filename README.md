# C JVM

Basic implementation of a JVM in C because I like it better than [Rust](https://github.com/santiago0411/Rust-JVM) (just don't leak memory lol).

Reads a .class file and executes its instructions.

Currently it supports:
 ```java
public class HelloWorld {
    public static void main(String[] args) {
        System.out.println("Hello World!");
        System.out.println(multiply(5, 4));
    }

    private static int multiply(int n1, int n2) {
        int sum = 0;
        for (int i = 0; i < n2; i++) {
            sum += n1;
        }
        return sum;
    }
}
```

Oracle JVM specifications:
- [Class File Format](https://docs.oracle.com/javase/specs/jvms/se7/html/jvms-4.html)
- [JVM Instruction Set](https://docs.oracle.com/javase/specs/jvms/se7/html/jvms-6.html)

### You code in Java, I code Java. We are not the same.
