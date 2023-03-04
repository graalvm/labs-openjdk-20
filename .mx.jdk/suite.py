suite = {
    "mxversion" : "6.12.0",
    "name" : "jdk",

    # ------------- Libraries -------------

    "libraries" : {

        "TESTNG" : {
            "sha1" : "6feb3e964aeb7097aff30c372aac3ec0f8d87ede",
            "maven" : {
                "groupId" : "org.testng",
                "artifactId" : "testng",
                "version" : "6.9.10",
            },
        },
    },

    "outputRoot" : "build/mxbuild",

    "projects" : {

        "jdk.internal.vm.ci" : {
            "subDir" : "src",
            "sourceDirs" : ["share/classes"],
            "javaCompliance" : "20+",
            "checkstyleVersion" : "8.36.1",
            "checkPackagePrefix" : False
        },

        "jdk.vm.ci.code.test" : {
            "subDir" : "test/hotspot/jtreg/compiler/jvmci",
            "sourceDirs" : ["src"],
            "dependencies" : [
                "mx:JUNIT",
                "jdk.internal.vm.ci",
            ],
            "checkstyle" : "jdk.internal.vm.ci",
            "javaCompliance" : "20+",
        },

        "jdk.vm.ci.runtime.test" : {
            "subDir" : "test/hotspot/jtreg/compiler/jvmci",
            "sourceDirs" : ["src"],
            "requires" : [
                "java.instrument",
            ],
            "requiresConcealed" : {
                "java.base" : [
                    "jdk.internal.reflect",
                    "jdk.internal.misc",
                    "jdk.internal.org.objectweb.asm"
                ],
            },
            "dependencies" : [
                "mx:JUNIT",
                "TESTNG",
                "jdk.internal.vm.ci",
            ],
            "checkstyle" : "jdk.internal.vm.ci",
            "javaCompliance" : "20+",
        },

        "jdk.vm.ci.hotspot.test" : {
            "subDir" : "test/hotspot/jtreg/compiler/jvmci",
            "sourceDirs" : ["src"],
            "requiresConcealed" : {
                "java.base" : [
                    "jdk.internal.misc",
                    "jdk.internal.vm.annotation",
                    "jdk.internal.org.objectweb.asm",
                ],
            },
            "dependencies" : [
                "mx:JUNIT",
                "TESTNG",
                "jdk.internal.vm.ci",
            ],
            "checkstyle" : "jdk.internal.vm.ci",
            "javaCompliance" : "20+",
        },
    }
}
