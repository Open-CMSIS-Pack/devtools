#!/usr/bin/env groovy

@Library("cmsis")
import com.arm.dsg.cmsis.jenkins.ArtifactoryHelper

// artifactory info
def artifactory = new ArtifactoryHelper(this)

// tool folders
def baseFolder          = "tools/svdconv"
def baseFolderWin       = baseFolder.replaceAll('/', '\\\\')
def buildFolder         = 'build'
def target              = "SVDConv"
def unittest            = "SVDConvUnitTests"
def unittestFolder      = "tools/svdconv/Test/UnitTests"
def unittestFolderWin   = unittestFolder.replaceAll('/', '\\\\')

// pipeline controlling variables
def systems             = [ 'win64', 'linux', 'macos' ]
def runCoverage         = true
def runCoverity         = true
def runRobotTest        = true
def runSendLastNightly  = (env.JOB_BASE_NAME == 'nightly')
def runPromoteRelease   = (env.JOB_BASE_NAME == 'release')

// configuration to send e-mail notification
Map mail_config = [
    to: 'samuel.pelegrinellocaipers@arm.com',
    cc: '',
    bcc: ''
]

def authenticode(file, algo = "sha2") {
    if (isUnix()) {
        sh("python3 /opt/buildtools/authenticode-client.py -d ${algo} ${file}")
    } else {
        bat("python C:\\tools\\buildtools\\authenticode-client.py -d ${algo} ${file}")
    }
}

DOCKERINFO_LINUX = [:]
DOCKERINFO_WINDOWS_VS2015 = [:]
DOCKERINFO_WINDOWS_VS2019 = [:]
kubernetes_yaml = ''

def setKubernetesYaml() {
    kubernetes_yaml = """\
    apiVersion: v1
    kind: Pod
    spec:
        imagePullSecrets:
          - name: "${DOCKERINFO_LINUX['k8sPullSecret']}"
        securityContext:
          runAsUser: 1000
          runAsGroup: 1000
        containers:
          - name: devtools
            image: "${DOCKERINFO_LINUX['registryUrl']}/${DOCKERINFO_LINUX['image']}:${DOCKERINFO_LINUX['label']}"
            alwaysPullImage: true
            imagePullPolicy: Always
            command:
              - sleep
            args:
              - infinity
            resources:
              requests:
                cpu: 2
                memory: 2Gi
    """.stripIndent()
}

pipeline {
    agent none
    options {
        timestamps()
        timeout(time: 5, unit: 'HOURS')
        skipDefaultCheckout()
    }
    stages {
        stage('Prepare') {
            agent any
            steps {
                script {
                    DOCKERINFO_LINUX = readJSON text: params.DOCKERINFO_LINUX
                    echo "DOCKERINFO_LINUX: ${DOCKERINFO_LINUX}"

                    DOCKERINFO_WINDOWS_VS2015 = readJSON text: params.DOCKERINFO_WINDOWS_VS2015
                    echo "DOCKERINFO_WINDOWS_VS2015: ${DOCKERINFO_WINDOWS_VS2015}"

                    DOCKERINFO_WINDOWS_VS2019 = readJSON text: params.DOCKERINFO_WINDOWS_VS2019
                    echo "DOCKERINFO_WINDOWS_VS2019: ${DOCKERINFO_WINDOWS_VS2019}"

                    setKubernetesYaml()
                    echo "kubernetes_yaml = $kubernetes_yaml"
                }
            }
        }
        stage('Build') {
            parallel {
                stage ('Windows x86')  {
                    when {
                        expression { 'win32' in systems }
                        beforeOptions true
                    }
                    agent {
                        docker {
                            alwaysPull true
                            image "${DOCKERINFO_WINDOWS_VS2015['registryUrl']}/${DOCKERINFO_WINDOWS_VS2015['image']}:${DOCKERINFO_WINDOWS_VS2015['label']}"
                            label 'Windows-Container'
                            registryCredentialsId "${DOCKERINFO_WINDOWS_VS2015['registryCredentialsId']}"
                            registryUrl "https://${DOCKERINFO_WINDOWS_VS2015['registryUrl']}"
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee("build.log") {
                                bat("""
                                    call "C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\vcvarsall.bat" x86
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
                                tools: [msBuild(id: 'win32_msvc', name: 'Windows x86 (MSVC)', pattern: 'build.log')]
                            )

                            // TO BE ENABLED!
                            // script {
                            //     if (runSendLastNightly || runPromoteRelease) {
                            //         authenticode("${baseFolderWin}\\${target}\\windows32\\Release\\${target}.exe", "sha2")
                            //     }
                            // }

                            archiveArtifacts "${baseFolderWin}\\${target}\\windows32\\Release\\${target}.exe"
                            stash includes: "${baseFolderWin}\\${target}\\windows32\\Release\\${target}.exe, ${baseFolderWin}\\CMAKE_PROJECT_VERSION", name: "win_${target}"
                            // script {
                            //     artifactory.upload pattern: "${baseFolderWin}\\${target}\\windows32\\Release\\${target}.exe"
                            // }
                        }
                    }
                }

                stage ('Windows x64')  {
                    when {
                        expression { 'win64' in systems }
                        beforeOptions true
                    }
                    agent {
                        docker {
                            alwaysPull true
                            image "${DOCKERINFO_WINDOWS_VS2019['registryUrl']}/${DOCKERINFO_WINDOWS_VS2019['image']}:${DOCKERINFO_WINDOWS_VS2019['label']}"
                            label 'Windows-Container'
                            registryCredentialsId "${DOCKERINFO_WINDOWS_VS2019['registryCredentialsId']}"
                            registryUrl "https://${DOCKERINFO_WINDOWS_VS2019['registryUrl']}"
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee("build.log") {
                                bat("""
                                    call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat"
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

                            // script {
                            //     if (runSendLastNightly || runPromoteRelease) {
                            //         authenticode("${baseFolderWin}\\${target}\\windows64\\Release\\${target}.exe", "sha2")
                            //     }
                            // }

                            archiveArtifacts "${baseFolderWin}\\${target}\\windows64\\Release\\${target}.exe"
                            stash includes: "${baseFolderWin}\\${target}\\windows64\\Release\\${target}.exe, ${baseFolderWin}\\CMAKE_PROJECT_VERSION", name: "win64_${target}"
                            // script {
                            //     artifactory.upload pattern: "${baseFolderWin}\\${target}\\windows64\\Release\\${target}.exe"
                            // }
                        }
                    }
                }

                stage ('Linux')  {
                    when {
                        expression { 'linux' in systems }
                        beforeOptions true
                    }
                    agent {
                        kubernetes {
                            defaultContainer 'devtools'
                            slaveConnectTimeout 600
                            yaml kubernetes_yaml
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee("build.log") {
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

                            archiveArtifacts "${baseFolder}/${target}/linux64/Release/${target}"
                            stash includes: "${baseFolder}/${target}/linux64/Release/${target}, ${baseFolder}/CMAKE_PROJECT_VERSION", name: "lin_${target}"
                            // script {
                            //     artifactory.upload pattern: "${baseFolder}/${target}/linux64/Release/${target}"
                            // }
                        }
                    }
                }

                stage ('macOS')  {
                    when {
                        expression { 'macos' in systems }
                        beforeOptions true
                    }

                    agent { label 'macos'}

                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee("build.log") {
                                sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..")
                                sh("cmake --build . --parallel 4 --target ${target}")
                            }
                            recordIssues(
                                filters: [includeFile("${baseFolder}/.*")],
                                qualityGates: [[threshold: 1, type: 'DELTA', unstable: true]],
                                referenceJobName: "devtools/${baseFolder}/nightly",
                                tools: [clang(id: 'mac_clang', name: 'macOS (AppleClang)', pattern: 'build.log')]
                            )

                            archiveArtifacts "${baseFolder}/${target}/darwin64/Release/${target}"
                            stash includes: "${baseFolder}/${target}/darwin64/Release/${target}, ${baseFolder}/CMAKE_PROJECT_VERSION", name: "mac_${target}"
                            // script {
                            //     artifactory.upload pattern: "${baseFolder}/${target}/darwin64/Release/${target}"
                            // }
                        }
                    }
                    post { always { cleanWs() } }
                }
            }
        }

        stage('Tests') {
            parallel {
                stage ('Unit_Tests_Win_x86')  {
                    when {
                        expression { ('win32' in systems) && unittest }
                        beforeOptions true
                    }
                    agent {
                        docker {
                            alwaysPull true
                            image "${DOCKERINFO_WINDOWS_VS2015['registryUrl']}/${DOCKERINFO_WINDOWS_VS2015['image']}:${DOCKERINFO_WINDOWS_VS2015['label']}"
                            label 'Windows-Container'
                            registryCredentialsId "${DOCKERINFO_WINDOWS_VS2015['registryCredentialsId']}"
                            registryUrl "https://${DOCKERINFO_WINDOWS_VS2015['registryUrl']}"
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee("build.log") {
                                bat("""
                                    call "C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\vcvarsall.bat" x86
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
                                tools: [msBuild(id: 'win32_ut', name: 'Windows-32 (Unit Test)', pattern: 'build.log')]
                            )

                            // Run unit Tests
                            bat("ctest -V -R ${unittest} -C Debug")
                            stash includes: "${unittestFolderWin}\\test_report_windows32.xml", name: 'test_report_win32'
                            archiveArtifacts "${unittestFolderWin}\\test_report_windows32.xml"
                        }
                    }
                }

                stage ('Unit_Tests_Win_x64')  {
                    when {
                        expression { ('win64' in systems) && unittest }
                        beforeOptions true
                    }
                    agent {
                        docker {
                            alwaysPull true
                            image "${DOCKERINFO_WINDOWS_VS2019['registryUrl']}/${DOCKERINFO_WINDOWS_VS2019['image']}:${DOCKERINFO_WINDOWS_VS2019['label']}"
                            label 'Windows-Container'
                            registryCredentialsId "${DOCKERINFO_WINDOWS_VS2019['registryCredentialsId']}"
                            registryUrl "https://${DOCKERINFO_WINDOWS_VS2019['registryUrl']}"
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee('build.log') {
                                bat("""
                                    call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat"
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
                            bat("ctest -V -R ${unittest} -C Debug")
                            stash includes: "${unittestFolderWin}\\test_report_windows64.xml", name: 'test_report_win64'
                            archiveArtifacts "${unittestFolderWin}\\test_report_windows64.xml"
                        }
                    }
                }

                stage ('Unit_Tests_Linux')  {
                    when {
                        expression { ('linux' in systems) && unittest }
                        beforeOptions true
                    }
                    agent {
                        kubernetes {
                            defaultContainer 'devtools'
                            slaveConnectTimeout 600
                            yaml kubernetes_yaml
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee("build.log") {
                                sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..")
                                sh("cmake --build . --parallel 4 --target ${unittest} 2>&1 | tee build_log")
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
                            sh("ctest -V -R ${unittest}")
                            stash includes: "${unittestFolder}/test_report_linux64.xml", name: 'test_report_lin'
                            archiveArtifacts "${unittestFolder}/test_report_linux64.xml"
                        }
                    }
                }

                stage ('Unit_Tests_macOS')  {
                    when {
                        expression { ('macos' in systems) && unittest }
                        beforeOptions true
                    }
                    agent { label 'macos'}
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee("build.log") {
                                sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..")
                                sh("cmake --build . --parallel 4 --target ${unittest} 2>&1 | tee build_log")
                            }
                            recordIssues(
                                filters: [includeFile("${baseFolder}/.*")],
                                qualityGates: [[threshold: 1, type: 'DELTA', unstable: true]],
                                referenceJobName: "devtools/${baseFolder}/nightly",
                                tools: [clang(id: 'mac_clang_ut', name: 'macOS (Unit Test)', pattern: 'build.log')]
                            )

                            // Run unit tests
                            sh("ctest -V -R ${unittest}")
                            stash includes: "${unittestFolder}/test_report_darwin64.xml", name: 'test_report_mac'
                            archiveArtifacts "${unittestFolder}/test_report_darwin64.xml"
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage ('Robot_Tests_Win_x86')  {
                    when {
                        expression { ('win32' in systems) && runRobotTest }
                        beforeAgent true
                    }
                    agent {
                        docker {
                            alwaysPull true
                            image "${DOCKERINFO_WINDOWS_VS2015['registryUrl']}/${DOCKERINFO_WINDOWS_VS2015['image']}:${DOCKERINFO_WINDOWS_VS2015['label']}"
                            label 'Windows-Container'
                            registryCredentialsId "${DOCKERINFO_WINDOWS_VS2015['registryCredentialsId']}"
                            registryUrl "https://${DOCKERINFO_WINDOWS_VS2015['registryUrl']}"
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            bat("""
                                call "C:\\Program Files (x86)\\Microsoft Visual Studio 14.0\\VC\\vcvarsall.bat" x86
                                cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
                            """)
                            unstash "win_${target}"

                            script {
                                // TO BE REMOVED
                                bat("pip3 install robotframework~=3.2.2")

                                def rc = bat(returnStatus: true, script: "ctest -V -R SVDConvTests")
                                if(rc) {
                                    unstable("Failing test cases detected!")
                                    zip zipFile: 'results.zip', archive: true, glob: '',
                                        dir: "${baseFolderWin}\\${target}\\windows32\\robot"
                                }
                            }
                            stash includes: "${baseFolderWin}\\${target}\\windows32\\robot\\output.xml", name: "win32_robot_result"
                        }
                    }
                }

                stage ('Robot_Tests_Win_x64')  {
                    when {
                        expression { ('win64' in systems) && runRobotTest }
                        beforeAgent true
                    }
                    agent {
                        docker {
                            alwaysPull true
                            image "${DOCKERINFO_WINDOWS_VS2019['registryUrl']}/${DOCKERINFO_WINDOWS_VS2019['image']}:${DOCKERINFO_WINDOWS_VS2019['label']}"
                            label 'Windows-Container'
                            registryCredentialsId "${DOCKERINFO_WINDOWS_VS2019['registryCredentialsId']}"
                            registryUrl "https://${DOCKERINFO_WINDOWS_VS2019['registryUrl']}"
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            bat("""
                                call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat"
                                cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
                            """)
                            unstash "win64_${target}"

                            // TO BE REMOVED
                            bat("pip3 install robotframework~=3.2.2")
                            script {
                                def rc = bat(returnStatus: true, script: "ctest -V -R SVDConvTests")
                                if(rc) {
                                    unstable("Failing test cases detected!")
                                    zip zipFile: 'results.zip', archive: true, glob: '',
                                        dir: "${baseFolderWin}\\${target}\\windows64\\robot"
                                }
                            }

                            stash includes: "${baseFolderWin}\\${target}\\windows64\\robot\\output.xml", name: "win64_robot_result"
                        }
                    }
                }

                stage ('Robot_Tests_Linux')  {
                    when {
                        expression { ('linux' in systems) && runRobotTest }
                        beforeOptions true
                    }
                    agent {
                        kubernetes {
                            defaultContainer 'devtools'
                            slaveConnectTimeout 600
                            yaml kubernetes_yaml
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..")
                            unstash "lin_${target}"

                            script {
                                def rc = sh(returnStatus: true, script: "ctest -V -R SVDConvTests")
                                if(rc) {
                                    unstable("Failing test cases detected!")
                                    zip zipFile: 'results.zip', archive: true, glob: '',
                                        dir: "${baseFolder}/${target}/linux64/robot"
                                }
                            }

                            stash includes: "${baseFolder}/${target}/linux64/robot/output.xml", name: "lin_robot_result"
                        }
                    }
                }

                stage ('Robot_Tests_macOS')  {
                    when {
                        expression { ('macos' in systems) && runRobotTest }
                        beforeOptions true
                    }
                    agent { label 'macos' }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..")
                            unstash "mac_${target}"

                            script {
                                def rc = sh(returnStatus: true, script: "ctest -V -R SVDConvTests")
                                if(rc) {
                                    unstable("Failing test cases detected!")
                                    zip zipFile: 'results.zip', archive: true, glob: '',
                                        dir: "${baseFolder}/${target}/darwin64/robot"
                                }
                            }

                            stash includes: "${baseFolder}/${target}/darwin64/robot/output.xml", name: "darwin_robot_result"
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage ('Code Coverage')  {
                    when {
                        expression { ('linux' in systems) && unittest && runCoverage }
                        beforeAgent true
                    }
                    agent {
                        kubernetes {
                            defaultContainer 'devtools'
                            slaveConnectTimeout 600
                            yaml kubernetes_yaml
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=on ..")
                            sh("cmake --build . --parallel 4 --target ${unittest}")

                            // Run unit tests
                            sh("ctest -V -R ${unittest}")
                            stash includes: "${unittestFolder}/test_report_linux64_cov.xml", name: 'test_report_cov'
                            archiveArtifacts "${unittestFolder}/test_report_linux64_cov.xml"

                            // Code Coverage Cobertura
                            sh("gcovr -r ../${baseFolder} -b --xml gcovr.xml --xml-pretty --exclude-directories ${unittestFolder} ${baseFolder}")
                            cobertura autoUpdateHealth: false,
                                autoUpdateStability: false,
                                coberturaReportFile: 'gcovr.xml',
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
                }

                stage('Coverity') {
                    when {
                        expression { return runCoverity }
                        beforeOptions true
                    }
                    agent {
                        kubernetes {
                            defaultContainer 'devtools'
                            slaveConnectTimeout 600
                            yaml kubernetes_yaml
                        }
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("${buildFolder}") {
                            // Coverity configure to use GCC/G+
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

        stage('Test Reports') {
            when {
                expression { return unittest }
                beforeOptions true
            }
            agent {
                kubernetes {
                    defaultContainer 'devtools'
                    slaveConnectTimeout 600
                    yaml kubernetes_yaml
                }
            }
            steps {
                script {
                    if ('win32' in systems) { unstash 'test_report_win32' }
                    if ('win64' in systems) { unstash 'test_report_win64' }
                    if ('linux' in systems) { unstash 'test_report_lin'
                        if (runCoverage)    { unstash 'test_report_cov'   }
                    }
                    if ('macos' in systems) { unstash 'test_report_mac'   }
                }
                xunit([GoogleTest(deleteOutputFiles: true,
                                failIfNotNew: true,
                                pattern: "${unittestFolder}/test_report_*.xml",
                                skipNoTestFiles: false,
                                stopProcessingIfError: true)])
            }

        }

        stage('Robot Test Reports') {
            when {
                expression { return runRobotTest }
                beforeOptions true
            }
            agent {
                docker {
                    alwaysPull true
                    image "${DOCKERINFO_WINDOWS_VS2019['registryUrl']}/${DOCKERINFO_WINDOWS_VS2019['image']}:${DOCKERINFO_WINDOWS_VS2019['label']}"
                    label 'Windows-Container'
                    registryCredentialsId "${DOCKERINFO_WINDOWS_VS2019['registryCredentialsId']}"
                    registryUrl "https://${DOCKERINFO_WINDOWS_VS2019['registryUrl']}"
                }
            }
            steps {
                script {
                    if ('win32' in systems) { unstash 'win32_robot_result' }
                    if ('win64' in systems) { unstash 'win64_robot_result' }
                    if ('linux' in systems) { unstash 'lin_robot_result'   }
                    if ('macos' in systems) { unstash 'darwin_robot_result'   }
                }

                bat("rebot --outputdir ${baseFolderWin}/${target} --output output.xml --name Collective_Robot_Results --nostatusrc \
                     ${baseFolderWin}/${target}/windows64/robot/output.xml \
                     ${baseFolder}/${target}/linux64/robot/output.xml \
                     ${baseFolder}/${target}/darwin64/robot/output.xml")
                    //  ${baseFolderWin}/${target}/windows32/robot/output.xml \
                robot outputPath: "${baseFolderWin}/${target}", passThreshold: 100.0
                script { artifactory.publishBuildInfo() }
            }

        }

        stage("Sending SVDConv to the Last Successful Nightly build location") {
            when {
                expression { return runSendLastNightly }
                beforeOptions true
            }
            agent {
                kubernetes {
                    defaultContainer 'devtools'
                    slaveConnectTimeout 600
                    yaml kubernetes_yaml
                }
            }
            steps {
                dir('binaries_to_artifactory') {
                    script {
                        if ('win32' in systems) { unstash "win_${target}" }
                        if ('win64' in systems) { unstash "win64_${target}" }
                        if ('linux' in systems) { unstash "lin_${target}" }
                        if ('macos' in systems) { unstash "mac_${target}" }

                        load("${baseFolder}/CMAKE_PROJECT_VERSION")
                        assert(CMAKE_PROJECT_NAME == target)

                        dir("${baseFolder}/${target}") {
                            retry(3) {
                                if ('win32' in systems) {
                                    artifactory.upload pattern: "windows32/Release/${target}.exe",
                                                       target: "mcu.cmsis/ci/artifacts/${env.JENKINS_ENV}/devtools/${baseFolder}/lastSuccessfulNightly/",
                                                       props: "os=windows32;version=${CMAKE_PROJECT_VERSION_FULL};"
                                }
                                if ('win64' in systems) {
                                    artifactory.upload pattern: "windows64/Release/${target}.exe",
                                                       target: "mcu.cmsis/ci/artifacts/${env.JENKINS_ENV}/devtools/${baseFolder}/lastSuccessfulNightly/",
                                                       props: "os=windows64;version=${CMAKE_PROJECT_VERSION_FULL};"
                                }
                                if ('linux' in systems) {
                                    artifactory.upload pattern: "linux64/Release/${target}",
                                                       target: "mcu.cmsis/ci/artifacts/${env.JENKINS_ENV}/devtools/${baseFolder}/lastSuccessfulNightly/",
                                                       props: "os=linux64;version=${CMAKE_PROJECT_VERSION_FULL};"
                                }
                                if ('macos' in systems) {
                                    artifactory.upload pattern: "darwin64/Release/${target}",
                                                       target: "mcu.cmsis/ci/artifacts/${env.JENKINS_ENV}/devtools/${baseFolder}/lastSuccessfulNightly/",
                                                       props: "os=darwin64;version=${CMAKE_PROJECT_VERSION_FULL};"
                                }
                            }
                        }
                    }
                }
            }
        }

        stage("Promoting Release") {
            when {
                expression { return runPromoteRelease }
                beforeOptions true
            }
            agent {
                kubernetes {
                    defaultContainer 'devtools'
                    slaveConnectTimeout 600
                    yaml kubernetes_yaml
                }
            }
            steps {
                dir('binaries_to_artifactory') {
                    script {
                        if ('win32' in systems) { unstash "win_${target}" }
                        if ('win64' in systems) { unstash "win64_${target}" }
                        if ('linux' in systems) { unstash "lin_${target}" }
                        if ('macos' in systems) { unstash "mac_${target}" }

                        load("${baseFolder}/CMAKE_PROJECT_VERSION")
                        assert(CMAKE_PROJECT_NAME == target)

                        dir("${baseFolder}/${target}") {
                            def releaseFolder = "mcu.promoted/${env.JENKINS_ENV == 'production' ? 'releases' : 'staging'}/devtools/${baseFolder}/${CMAKE_PROJECT_VERSION}"

                            retry(3) {
                                if ('win32' in systems) {
                                    artifactory.upload pattern: "windows32/Release/${target}.exe", target: "${releaseFolder}/windows32/", flat: true,
                                                       props: "os=windows32;version=${CMAKE_PROJECT_VERSION_FULL};"
                                }
                                if ('win64' in systems) {
                                    artifactory.upload pattern: "windows64/Release/${target}.exe", target: "${releaseFolder}/windows64/", flat: true,
                                                       props: "os=windows64;version=${CMAKE_PROJECT_VERSION_FULL};"
                                }
                                if ('linux' in systems) {
                                    artifactory.upload pattern: "linux64/Release/${target}", target: "${releaseFolder}/linux64/", flat: true,
                                                       props: "os=linux64;version=${CMAKE_PROJECT_VERSION_FULL};"
                                }
                                if ('macos' in systems) {
                                    artifactory.upload pattern: "darwin64/Release/${target}", target: "${releaseFolder}/darwin64/", flat: true,
                                                       props: "os=darwin64;version=${CMAKE_PROJECT_VERSION_FULL};"
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    post {
        unsuccessful {
            // only send email when running it in production CI and build job is nightly.
            mailMcu(mail_config)
        }
    }
}
