/* @test /nodynamiccopyright/
 * @bug 8037385
 * @summary Must not allow static interface method invocation in legacy code
 * @compile -Xlint:-options StaticInvokeSimple.java
 */
import java.util.stream.Stream;

class StaticInvokeSimple {
    void test() {
        Stream.empty();
    }
}
