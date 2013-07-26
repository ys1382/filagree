import java.lang.reflect.Method;

public class JniSample {

    public native String sayHello(String name);

    public static void x() { System.out.println("I am X!"); }
    public static void y() { System.out.println("I am Y!"); }
    
    public static void main(String[] args) {

        System.loadLibrary("JniSample");
        System.out.println("In java main");

        JniSample s = new JniSample();
        String result = s.sayHello("world!");
        System.out.println("in Java, the result is " + result);

        try {
            Class<?> c = Class.forName("JniSample");
            Class[] argTypes = new Class[0];
            Method callback = c.getDeclaredMethod(result, argTypes);
            Object[] invokeArgs = new Object[0];
            callback.invoke(null, invokeArgs);

        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
