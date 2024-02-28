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