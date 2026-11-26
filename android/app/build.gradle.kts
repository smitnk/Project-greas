plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.smitnk.projectgrease"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.smitnk.projectgrease"
        minSdk = 26
        targetSdk = 35
        versionCode = 37
        versionName = "0.060 beta"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

    }

    buildTypes {
        // debug: arm64-v8a for phones, x86_64 for the emulator job (instrumented tests)
        debug {
            ndk { abiFilters += listOf("arm64-v8a", "x86_64") }
        }
        // release (phones): arm64-v8a only; the packager strips the .so files, the unstripped
        // ones are kept by CI (android-shell.yml) for crash symbolication
        release {
            isMinifyEnabled = false
            ndk { abiFilters += listOf("arm64-v8a") }
        }
    }

    // lintVitalAnalyzeRelease crashed in lifecycle's NullSafeMutableLiveData detector (built against
    // another Kotlin analysis API: IncompatibleClassChangeError), failing assembleRelease in CI.
    lint { disable += "NullSafeMutableLiveData" }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        compose = true
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }

    ndkVersion = "27.0.12077973"
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2026.02.01")
    implementation(composeBom)
    androidTestImplementation(composeBom)

    implementation("androidx.activity:activity-compose:1.10.1")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    debugImplementation("androidx.compose.ui:ui-tooling")

    testImplementation("junit:junit:4.13.2")

    // Instrumented emulator tests (DeviceBugfixTest): touches, native state, screenshots.
    androidTestImplementation("androidx.test:runner:1.6.2")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.compose.ui:ui-test-junit4")
    debugImplementation("androidx.compose.ui:ui-test-manifest")
    // Real org.json for the project-file round-trip test (the Android stub jar throws "not mocked").
    testImplementation("org.json:json:20231013")
}
