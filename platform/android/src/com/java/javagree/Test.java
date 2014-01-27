package com.java.javagree;

import java.util.HashMap;
import java.util.Iterator;

public class Test {

	final public static String test_ui =
			"ctx = []"
			+ "ctx.clicked = function(ctx) sys.ui_set(ctx.ui.lbl, 'ouch') end "
			+ "ctx.ui = sys.ui( ctx, "
			+ "['vertical', "
			+ "	['label', 'name':'lbl', 'text':'do not'], "
			+ "	['button', 'text':'click', 'name':'bttn', 'logic':ctx.clicked], "
			+ " ['input'],"
			+ " ['table', 'list':['7','8','9']],"
			+ "])";

	void test1(Javagree sys) {
		sys.eval("sys.print('fg gets ' + tc.x + tc.z([7,8,9], ['p':99]))");
	}

	void test2(Javagree jg, HalAndroid sys) {
		String imports = sys.read("ui.fg");
		jg.eval(imports + Test.test_ui);
	}

	void test3(Javagree jg, HalAndroid sys) {
		String s1 = sys.read("mesh.fg");
		String s2 = sys.read("ui.fg");
		String s3 = sys.read("sync/sync.fg");
		String s4 = sys.read("sync/sync_client.fg");
		String source = s1 + s2 + s3 +"id='Android' host='10.0.2.2'" + s4;
		jg.eval(source);
	}

	void doTesting() {
		HalAndroid sys = new HalAndroid();
		Javagree jg = new Javagree(this, "tc", sys);
		sys.setJavagree(jg);

		//test1(jg);
		//test2(jg,sys);
		test3(jg, sys);
	}

	public int x = 6;

	public Integer y(Object a, Object b) {
		System.out.println("y: " + a +","+ b);
		return 99;
	}

	public Integer z(Object list, Object map) {

		System.out.println("list:");
		Object[] list2 = (Object[])list;
		for (int i=0; i<list2.length; i++)
			System.out.println("\t" + list2[i]);

		System.out.println("map:");
		HashMap<?,?> map2 = (HashMap<?,?>)map;
		Iterator<?> iterator = map2.keySet().iterator();
		while (iterator.hasNext()) {
			Object key = iterator.next();
			Object value = map2.get(key);
			System.out.println("\t" + key + " " + value);
		}

		return list2.length;
	}
}
