import java.net.URI
import java.util.Locale

buildscript {
    repositories {
        google()
        mavenCentral()
    }
}

plugins {
    id("com.android.library")
}

repositories {
    google()
    mavenCentral()
    maven { url = URI("https://www.jitpack.io") } // MPAndroidChart
}

val osName = System.getProperties()["os.name"].toString().lowercase(Locale.getDefault())

android {
    namespace = "app.organicmaps.sdk"
    compileSdk = 34

    defaultConfig {
        minSdk = 21

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        consumerProguardFiles("consumer-rules.pro")

        externalNativeBuild {
            var pchFlag = "OFF"
            if (project.hasProperty("pch")) pchFlag = "ON"

            var njobs = ""
//            if (project.hasProperty("njobs")) njobs = project.getProperty("njobs")

            var enableVulkanDiagnostics = "OFF"
//            if (project.hasProperty("enableVulkanDiagnostics")) {
//                enableVulkanDiagnostics = project.getProperty("enableVulkanDiagnostics")
//            }

            cmake {
                cppFlags("-fexceptions", "-frtti")
                // There is no sense to enable sections without gcc's --gc-sections flag.
                cFlags("-fno-function-sections", "-fno-data-sections", "-Wno-extern-c-compat")
                arguments("-DANDROID_TOOLCHAIN=clang", "-DANDROID_STL=c++_static",
                        "-DOS=$osName", "-DSKIP_TESTS=ON", "-DSKIP_TOOLS=ON", "-DUSE_PCH=$pchFlag",
                        "-DNJOBS=$njobs", "-DENABLE_VULKAN_DIAGNOSTICS=$enableVulkanDiagnostics")
                targets("organicmaps")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }
    externalNativeBuild {
        cmake {
            path("../../CMakeLists.txt")
            version = "3.22.1+"
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

dependencies {

    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.12.0")
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
}