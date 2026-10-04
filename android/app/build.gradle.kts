plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

// Bug-hunt build (.github/workflows/bughunt.yml): -Ppg.bughunt=true turns the debug build into the
// hunting build: native AddressSanitizer (x86_64, wrap.sh + the NDK's ASan runtime), a glGetError check
// after every GLES call, StrictMode and LeakCanary's test listener. -Ppg.abis=x86_64 limits the ABIs.
val bughunt = (findProperty("pg.bughunt") as String?)?.toBoolean() ?: false
val abiList = (findProperty("pg.abis") as String?)?.split(",")?.map { it.trim() }?.filter { it.isNotEmpty() }
    ?: listOf("arm64-v8a", "x86_64")

android {
    namespace = "com.smitnk.projectgrease"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.smitnk.projectgrease"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        // arm64-v8a for devices, x86_64 for the emulator job (instrumented device-bugfix tests).
        ndk {
            abiFilters += abiList
        }
        buildConfigField("boolean", "BUGHUNT", bughunt.toString())
        if (bughunt) {
            externalNativeBuild {
                cmake {
                    // ASan wants the shared C++ runtime (wrap.sh preloads it after the ASan runtime).
                    arguments += listOf("-DPG_ASAN=ON", "-DPG_GL_DEBUG=ON", "-DANDROID_STL=c++_shared")
                }
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    if (bughunt) {
        // wrap.sh (lib/<abi>/wrap.sh) only runs with extracted native libraries.
        packaging { jniLibs { useLegacyPackaging = true } }
        sourceSets["debug"].resources.srcDir("src/bughunt/resources")
        sourceSets["debug"].jniLibs.srcDir(layout.buildDirectory.get().asFile.resolve("bughunt/jniLibs"))
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }

    ndkVersion = "27.0.12077973"
}

// The NDK's ASan runtime for the emulator ABI goes next to the app's native library.
val copyAsanRuntime by tasks.registering(Copy::class) {
    val ndk = android.ndkDirectory
    from(fileTree(ndk) { include("toolchains/llvm/prebuilt/*/lib/clang/*/lib/linux/libclang_rt.asan-x86_64-android.so") })
    eachFile { path = name }
    includeEmptyDirs = false
    into(layout.buildDirectory.dir("bughunt/jniLibs/x86_64"))
}
if (bughunt) {
    tasks.matching { it.name == "mergeDebugJniLibFolders" }.configureEach { dependsOn(copyAsanRuntime) }
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2026.02.01")
    implementation(composeBom)
    androidTestImplementation(composeBom)

    implementation("androidx.activity:activity-compose:1.10.1")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    debugImplementation("androidx.compose.ui:ui-tooling")

    testImplementation("junit:junit:4.13.2")

    // Instrumented emulator tests (DeviceBugfixTest): touches, native state, screenshots.
    androidTestImplementation("androidx.test:runner:1.6.2")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.compose.ui:ui-test-junit4")
    debugImplementation("androidx.compose.ui:ui-test-manifest")
    if (bughunt) {
        // Fails a test whose activity / views / controller leak after it (FailTestOnLeakRunListener).
        androidTestImplementation("com.squareup.leakcanary:leakcanary-android-instrumentation:2.14")
    }
    // Real org.json for the project-file round-trip test (the Android stub jar throws "not mocked").
    testImplementation("org.json:json:20231013")
}
