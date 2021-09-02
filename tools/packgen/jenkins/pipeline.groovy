#!/usr/bin/env groovy

@Library("cmsis")
import com.arm.dsg.cmsis.jenkins.ArtifactoryHelper

// artifactory info
def artifactory = new ArtifactoryHelper(this)

// tool folders
def baseFolder          = "tools/packgen"
def baseFolderWin       = baseFolder.replaceAll('/', '\\\\')
def buildFolder         = 'build'
def target              = "packgen"
def unittest            = "PackGenUnitTests"

// pipeline controlling variables
def systems             = [ 'win64', 'linux', 'macos' ]
def runCoverage         = true
def runCoverity         = true
def runSendLastNightly  = (env.JOB_BASE_NAME == 'nightly')

// configuration to send e-mail notification
Map mail_config = [
    to: 'samuel.pelegrinellocaipers@arm.com',
    cc: 'daniel.brondani@arm.com',
    bcc: ''
]

pipeline {
    agent none
    options {
        timestamps()
        timeout(time: 5, unit: 'HOURS')
        skipDefaultCheckout()
    }
    stages {
        stage('Build') {
            parallel {
                stage ('Windows x64')  {
                    when { expression { 'win64' in systems } }
                    agent { label 'Windows-MDK' }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee('build.log') {
                                bat("""
                                    call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat"
                                    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
                                    if %errorlevel% neq 0 exit /b %errorlevel%
                                    cmake --build . --parallel 4 --target ${target}
                                    exit /b %errorlevel%
                                """)
                            }
                            recordIssues(
                                filters: [includeFile("${baseFolder}/.*")],
                                qualityGates: [[threshold: 1, type: 'DELTA', unstable: true]],
                                referenceJobName: "devtools/${baseFolder}/nightly",
                                tools: [msBuild(id: 'win64_msvc', name: 'Windows x64 (MSVC)', pattern: 'build.log')]
                            )

                            archiveArtifacts "${baseFolderWin}\\windows64\\Release\\${target}.exe"
                            stash includes: "${baseFolderWin}\\windows64\\Release\\${target}.exe, ${baseFolderWin}\\CMAKE_PROJECT_VERSION", name: "win64_${target}"
                            script {
                                artifactory.upload pattern: "${baseFolderWin}\\windows64\\Release\\${target}.exe"
                            }
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage ('Linux')  {
                    when { expression { 'linux' in systems } }
                    agent {
                        docker {
                            image "${DOCKER_TAG}"
                            label "Linux"
                            registryUrl "https://${DOCKER_REGISTRY}"
                            registryCredentialsId "${DOCKER_REGISTRY_CREDENTIALS_ID}"
                            alwaysPull true
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee('build.log') {
                                sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..")
                                sh("cmake --build . --parallel 4 --target ${target}")
                            }

                            // The file build.log initially is stored on master and it needs to be
                            // transfer to the remote (agent) eventually. Based on my tests,
                            // this sync is not working properly for Linux agents.
                            // The complete log is not present in the agent when running recordIssues.
                            // The sleep & sync command intend to workaround it.
                            // We need to see if this will be enough.
                            sleep 5
                            sh('sync')
                            archiveArtifacts "build.log"

                            recordIssues(
                                filters: [includeFile("${baseFolder}/.*")],
                                qualityGates: [[threshold: 1, type: 'DELTA', unstable: true]],
                                referenceJobName: "devtools/${baseFolder}/nightly",
                                tools: [gcc(id: 'linux_gcc', name: 'Linux (GCC)', pattern: 'build.log')]
                            )

                            archiveArtifacts "${baseFolder}/linux64/Release/${target}"
                            stash includes: "${baseFolder}/linux64/Release/${target}, ${baseFolder}/CMAKE_PROJECT_VERSION", name: "lin_${target}"
                            script {
                                artifactory.upload pattern: "${baseFolder}/linux64/Release/${target}"
                            }
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage ('macOS')  {
                    when {
                        beforeAgent true
                        expression { 'macos' in systems }
                    }
                    agent { label 'macos'}
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee('build.log') {
                                sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..")
                                sh("cmake --build . --parallel 4 --target ${target}")
                            }

                            recordIssues(
                                filters: [includeFile("${baseFolder}/.*")],
                                qualityGates: [[threshold: 1, type: 'DELTA', unstable: true]],
                                referenceJobName: "devtools/${baseFolder}/nightly",
                                tools: [clang(id: 'mac_clang', name: 'macOS', pattern: 'build.log')]
                            )

                            archiveArtifacts "${baseFolder}/darwin64/Release/${target}"
                            stash includes: "${baseFolder}/darwin64/Release/${target}, ${baseFolder}/CMAKE_PROJECT_VERSION", name: "mac_${target}"
                            script {
                                artifactory.upload pattern: "${baseFolder}/darwin64/Release/${target}"
                            }
                        }
                    }
                    post { always { cleanWs() } }
                }
            }
        }

        stage('Unit Test') {
            parallel {
                stage ('Windows x64')  {
                    when { expression { ('win64' in systems) && unittest } }
                    agent { label 'Windows-MDK' }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee('build.log') {
                                bat("""
                                    call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat"
                                    cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
                                    if %errorlevel% neq 0 exit /b %errorlevel%
                                    cmake --build . --parallel 4 --target ${unittest}
                                    exit /b %errorlevel%
                                """)
                            }
                            recordIssues(
                                filters: [includeFile("${baseFolder}/.*")],
                                qualityGates: [[threshold: 1, type: 'DELTA', unstable: true]],
                                referenceJobName: "devtools/${baseFolder}/nightly",
                                tools: [msBuild(id: 'win64_ut', name: 'Windows-64 (Unit Test)', pattern: 'build.log')]
                            )

                            // Run unit tests
                            bat("""
                                call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat"
                                ${baseFolderWin}\\test\\windows64\\Debug\\${unittest}.exe --gtest_output=xml:test_report_win64.xml
                                exit /b %errorlevel%
                            """)
                            stash includes: "test_report_win64.xml", name: 'test_report_win64'
                            archiveArtifacts "test_report_win64.xml"
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage ('Linux')  {
                    when { expression { ('linux' in systems) && unittest } }
                    agent {
                        docker {
                            image "${DOCKER_TAG}"
                            label "Linux"
                            registryUrl "https://${DOCKER_REGISTRY}"
                            registryCredentialsId "${DOCKER_REGISTRY_CREDENTIALS_ID}"
                            alwaysPull true
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee('build.log') {
                                sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..")
                                sh("cmake --build . --parallel 4 --target ${unittest}")
                            }

                            // The file build.log initially is stored on master and it needs to be
                            // transfer to the remote (agent) eventually. Based on my tests,
                            // this sync is not working properly for Linux agents.
                            // The complete log is not present in the agent when running recordIssues.
                            // The sleep & sync command intend to workaround it.
                            // We need to see if this will be enough.
                            sleep 5
                            sh('sync')
                            archiveArtifacts "build.log"

                            recordIssues(
                                filters: [includeFile("${baseFolder}/.*")],
                                qualityGates: [[threshold: 1, type: 'DELTA', unstable: true]],
                                referenceJobName: "devtools/${baseFolder}/nightly",
                                tools: [gcc(id: 'lin_gcc_ut', name: 'Linux (Unit Test)', pattern: 'build.log')]
                            )

                            // Run unit tests
                            sh("${baseFolder}/test/linux64/Debug/${unittest} --gtest_output=xml:test_report_lin.xml")
                            stash includes: "test_report_lin.xml", name: 'test_report_lin'
                            archiveArtifacts "test_report_lin.xml"
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage ('macOS')  {
                   when {
                        beforeAgent true
                        expression { ('macos' in systems) && unittest }
                    }
                    agent { label 'macos'}
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee('build.log') {
                                sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..")
                                sh("cmake --build . --parallel 4 --target ${unittest}")
                            }
                            recordIssues(
                                filters: [includeFile("${baseFolder}/.*")],
                                qualityGates: [[threshold: 1, type: 'DELTA', unstable: true]],
                                referenceJobName: "devtools/${baseFolder}/nightly",
                                tools: [clang(id: 'mac_clang_ut', name: 'macOS (Unit Test)', pattern: 'build.log')]
                            )

                            // Run unit tests
                            sh("${baseFolder}/test/darwin64/Debug/${unittest} --gtest_output=xml:test_report_mac.xml")
                            stash includes: "test_report_mac.xml", name: 'test_report_mac'
                            archiveArtifacts "test_report_mac.xml"
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage ('Code Coverage')  {
                    when { expression { ('linux' in systems) && unittest && runCoverage } }
                    agent {
                        docker {
                            image "${DOCKER_TAG}"
                            label "Linux"
                            registryUrl "https://${DOCKER_REGISTRY}"
                            registryCredentialsId "${DOCKER_REGISTRY_CREDENTIALS_ID}"
                            alwaysPull true
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=on ..")
                            sh("cmake --build . --parallel 4 --target ${unittest}")

                            // Run unit tests
                            sh("./${baseFolder}/test/linux64/Debug/${unittest}")

                            // Code Coverage Cobertura
                            sh("gcovr -r ../${baseFolder}/src -b --xml -o cobertura.xml ${baseFolder}")
                            cobertura autoUpdateHealth: false,
                                autoUpdateStability: false,
                                coberturaReportFile: 'cobertura.xml',
                                conditionalCoverageTargets: '70, 0, 0',
                                failUnhealthy: false,
                                failUnstable: false,
                                lineCoverageTargets: '80, 0, 0',
                                maxNumberOfBuilds: 0,
                                methodCoverageTargets: '80, 0, 0',
                                onlyStable: true,
                                sourceEncoding: 'ASCII',
                                zoomCoverageChart: false
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage('Coverity') {
                    when {
                        expression { return runCoverity }
                        beforeOptions true
                    }
                    agent {
                        docker {
                            image "${DOCKER_TAG}"
                            label "Linux"
                            registryUrl "https://${DOCKER_REGISTRY}"
                            registryCredentialsId "${DOCKER_REGISTRY_CREDENTIALS_ID}"
                            alwaysPull true
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("${buildFolder}") {
                            sh("cov-configure --config cov_config_file --gcc")
                            sh("CC=gcc CXX=g++ cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..")
                            sh("cov-build --config cov_config_file --dir idir cmake --build . --parallel 4 --target ${target}")
                            sh("cov-analyze --dir idir --security")
                            sh("cov-format-errors --dir idir --json-output-v7 coverity.json --include-files '${baseFolder}/.*'")
                            sh("python3 ../scripts/coverity2jenkins.py -o coverity.log coverity.json")
                        }

                        recordIssues(
                            filters: [includeFile("${baseFolder}/.*")],
                            qualityGates: [[threshold: 1, type: 'NEW', unstable: true]],
                            referenceJobName: "devtools/${baseFolder}/nightly",
                            tools: [groovyScript(id: 'coverity', name: 'Coverity', parserId: 'coverity', pattern: "${buildFolder}/coverity.log")]
                        )
                    }
                    post { always { cleanWs() } }
                }
            }
        }

        stage('Artifactory publish buildinfo') {
            agent any
            steps { script { artifactory.publishBuildInfo() } }
        }

        stage('Test Reports') {
            when { expression { return unittest } }
            agent any
            steps {
                deleteDir()
                script {
                    if ('win64' in systems) { unstash 'test_report_win64' }
                    if ('linux' in systems) { unstash 'test_report_lin'   }
                    if ('macos' in systems) { unstash 'test_report_mac'   }
                }
                xunit([GoogleTest(deleteOutputFiles: true,
                                failIfNotNew: true,
                                pattern: "test_report_*.xml",
                                skipNoTestFiles: false,
                                stopProcessingIfError: true)])
            }
            post { always { cleanWs() } }
        }

        stage("Sending PackGen to the Last Successful Nightly build location") {
            agent { label 'Linux-MDK' }
            // Important: only run this stage when the expression is satisfied.
            when { expression { return runSendLastNightly } }
            steps {
                dir('binaries_to_artifactory') {
                    script {
                        if ('win64' in systems) { unstash "win64_${target}" }
                        if ('linux' in systems) { unstash "lin_${target}" }
                        if ('macos' in systems) { unstash "mac_${target}" }

                        dir("${baseFolder}") {
                            retry(3) {
                                if ('win64' in systems) {
                                    artifactory.upload pattern: "windows64/Release/${target}.exe",
                                                    target: "mcu.cmsis/ci/artifacts/${env.JENKINS_ENV}/devtools/${baseFolder}/lastSuccessfulNightly/"
                                }
                                if ('linux' in systems) {
                                    artifactory.upload pattern: "linux64/Release/${target}",
                                                    target: "mcu.cmsis/ci/artifacts/${env.JENKINS_ENV}/devtools/${baseFolder}/lastSuccessfulNightly/"
                                }
                                if ('macos' in systems) {
                                    artifactory.upload pattern: "darwin64/Release/${target}",
                                                    target: "mcu.cmsis/ci/artifacts/${env.JENKINS_ENV}/devtools/${baseFolder}/lastSuccessfulNightly/"
                                }
                            }
                        }
                    }
                }
            }
            post { always { cleanWs() } }
        }
    }

    post {
        unsuccessful {
            // only send email when running it in production CI and build job is nightly.
            mailMcu(mail_config)
        }
    }
}