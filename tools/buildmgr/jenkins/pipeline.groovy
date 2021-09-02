#!/usr/bin/env groovy

@Library("cmsis")
import com.arm.dsg.cmsis.jenkins.ArtifactoryHelper

// artifactory info
def artifactory = new ArtifactoryHelper(this)

// buildmgr folders
def baseFolder      = 'tools/buildmgr'
def binFolder       = "${baseFolder}/cbuildgen/distribution/bin"
def buildFolder     = 'build'
def docFolder       = "${baseFolder}/cbuildgen/distribution/doc"
def winBaseFolder   = baseFolder.replaceAll('/', '\\\\')
def linBaseFolder   = baseFolder
def linBinFolder    = binFolder
def linDocFolder    = docFolder

// cmsis-pack cache folders
def win_packs       = "D:\\packs"
def lin_packs       = "/home/jenkins/packs"
def mac_packs       = "/Users/jenkins/packs"

// stage modifiers
def runCoverity         = (env.JOB_BASE_NAME == 'nightly' || env.JOB_BASE_NAME == 'pre_commit') ? true : false
def runDynamicAnalysis  = (env.JOB_BASE_NAME == 'nightly') ? true : false
def runOTGtests         = true
def runSendLastNightly  = (env.JOB_BASE_NAME == 'nightly') ? true : false
def systems             = [ 'win64', 'linux', 'macos' ]

// configuration to send e-mail notification
Map mail_config = [
    to: 'samuel.pelegrinellocaipers@arm.com',
    cc: 'daniel.brondani@arm.com',
    bcc: ''
]

// pipeline step methods
def installerStep(Map args) {
    // args
    artifactory = args.artifactory
    linBinFolder = args.linBinFolder
    linBaseFolder = args.linBaseFolder

    // pipeline
    checkoutScmWithRetry(3)
    dir(linBaseFolder) {
      sh("mkdir -p cbuildgen/distribution/bin")
    }
    unstash 'docs'
    unzip dir: "${linBaseFolder}/cbuildgen/distribution/doc", zipFile: 'docs.zip'

    // FixMe: Below changes are temporary and needs to be changed in different patch for Installer change
    unstash 'win_cbuildgen'
    fileOperations([fileRenameOperation(destination: "${linBinFolder}/cbuildgen.exe", source: "${linBaseFolder}/cbuildgen/windows64/Release/cbuildgen.exe")])

    unstash 'lin_cbuildgen'
    fileOperations([fileRenameOperation(destination: "${linBinFolder}/cbuildgen.lin", source: "${linBaseFolder}/cbuildgen/linux64/Release/cbuildgen")])

    unstash 'mac_cbuildgen'
    fileOperations([fileRenameOperation(destination: "${linBinFolder}/cbuildgen.mac", source: "${linBaseFolder}/cbuildgen/darwin64/Release/cbuildgen")])

    dir("${linBaseFolder}/cbuildgen/installer") {
        sh("./create_installer.sh --input=../distribution --output=output")
        sh("yes | ./make_deb.sh --input=../distribution --output=output/debian")
    }

    zip dir: "${linBaseFolder}/cbuildgen/distribution", glob: '', zipFile: "${linBaseFolder}/cbuildgen/installer/output/cbuild_dist.zip"

    archiveArtifacts "${linBaseFolder}/cbuildgen/installer/output/cbuild_dist.zip"
    archiveArtifacts "${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
    archiveArtifacts "${linBaseFolder}/cbuildgen/installer/output/debian/cmsis-build*.deb"
    script {
        artifactory.upload pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_dist.zip"
        artifactory.upload pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
        artifactory.upload pattern: "${linBaseFolder}/cbuildgen/installer/output/debian/cmsis-build*.deb"
        artifactory.publishBuildInfo()
    }
}

pipeline {
    agent none
    options {
        timestamps()
        timeout(time: 2, unit: 'HOURS')
        skipDefaultCheckout()
    }
    environment {
        // CI_ACCOUNT is set as USERNAME:PASSWORD
        // In adition, it creates implicitly CI_ACCOUNT_USR and CI_ACCOUNT_PSW
        CI_ACCOUNT      = credentials('grasci')
        CI_SVN_USERNAME = '$CI_ACCOUNT_USR'
        CI_SVN_PASSWORD = '$CI_ACCOUNT_PSW'
        // variable for get_dependencies.sh
        USER            = '$CI_ACCOUNT_USR'
    }

    /**********************************
    Please DO NOT CHANGE the stage names
    They need to be static because of DSG-Metrics.
    ***********************************/
    stages {
        stage('Build') {
            parallel {
                stage ('Build_Windows_x64')  {
                    agent { label 'Windows-MDK' }
                    when { expression { 'win64' in systems } }
                    environment {
                        CI_PACK_ROOT             = "${win_packs}\\${EXECUTOR_NUMBER}\\"
                        CI_ARMCC5_TOOLCHAIN_ROOT = "${ARMCC5_PATH}\\bin"
                        CI_ARMCC6_TOOLCHAIN_ROOT = "${ARMCC6_PATH}\\bin"
                        CI_GCC_TOOLCHAIN_ROOT    = "${GCC_PATH}\\bin"
                        // Windows bash friendly path
                        GIT_SSH_COMMAND          = "ssh -i /C/users/jenkins/.ssh/id_rsa"
                        // Bug workaround https://gitlab.com/tortoisegit/tortoisegit/-/issues/3139
                        GIT_SSH_VARIANT          = "ssh"
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            tee("build.log") {
                                bat("""
                                    call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat"
                                    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
                                    if %errorlevel% neq 0 exit /b %errorlevel%
                                    cmake --build . --parallel 4 --target cbuildgen
                                    exit /b %errorlevel%
                                """)
                            }
                            stash includes: "${winBaseFolder}\\cbuildgen\\windows64\\Release\\cbuildgen.exe", name: 'win_cbuildgen'
                            recordIssues(
                                filters: [includeFile("${baseFolder}/.*")],
                                qualityGates: [[threshold: 1, type: 'DELTA', unstable: true]],
                                referenceJobName: "devtools/${baseFolder}/nightly",
                                tools: [msBuild(id: 'win_build', name: 'Windows x64 (Visual Studio)', pattern: 'build.log')]
                            )
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage('Build_Linux') {
                    agent {
                        docker {
                            image "${DOCKER_TAG}"
                            label "Linux"
                            registryUrl "https://${DOCKER_REGISTRY}"
                            registryCredentialsId "${DOCKER_REGISTRY_CREDENTIALS_ID}"
                            args "-v ${lin_packs}/:${lin_packs}/"
                            alwaysPull true
                        }
                    }
                    environment {
                        CI_PACK_ROOT = "${lin_packs}/${EXECUTOR_NUMBER}/"
                    }
                    when { expression { 'linux' in systems } }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..")
                            tee("build.log") {
                                sh("cmake --build . --parallel 4 --target cbuildgen")
                            }
                            stash includes: "${linBaseFolder}/cbuildgen/linux64/Release/cbuildgen, ${baseFolder}/CMAKE_PROJECT_VERSION", name: 'lin_cbuildgen'
                            recordIssues(
                                filters: [includeFile("${baseFolder}/.*")],
                                qualityGates: [[threshold: 1, type: 'DELTA', unstable: true]],
                                referenceJobName: "devtools/${baseFolder}/nightly",
                                tools: [gcc(id: 'linux_build', name: 'Linux (GCC)', pattern: 'build.log')]
                            )
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage('Build_MacOS') {
                    agent { label 'macos'}
                    when { expression { 'macos' in systems } }
                    environment {
                        CI_PACK_ROOT             = "${mac_packs}/${EXECUTOR_NUMBER}/"
                        CI_ARMCC5_TOOLCHAIN_ROOT = ""
                        CI_ARMCC6_TOOLCHAIN_ROOT = "${ARMCC6_PATH}/bin"
                        CI_GCC_TOOLCHAIN_ROOT    = "${GCC_PATH}/bin"
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        dir("build") {
                            sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..")
                            tee("build.log") {
                                sh("cmake --build . --parallel 4 --target cbuildgen")
                            }
                            stash includes: "${linBaseFolder}/cbuildgen/darwin64/Release/cbuildgen", name: 'mac_cbuildgen'
                            recordIssues(
                                filters: [includeFile("${baseFolder}/.*")],
                                qualityGates: [[threshold: 1, type: 'DELTA', unstable: true]],
                                referenceJobName: "devtools/${baseFolder}/nightly",
                                tools: [clang(id: 'mac_clang', name: 'macOS', pattern: 'build.log')]
                            )
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage('Generate_Docs') {
                    agent { label 'Linux' }
                    steps {
                        checkoutScmWithRetry(3)
                        tee('build.log') {
                            sh("cd ${linBaseFolder}/docs/doxygen && ./gen_doc.sh --output=../../cbuildgen/distribution/doc")
                        }
                        zip dir: "${linDocFolder}", glob: '', zipFile: 'docs.zip'
                        stash includes: 'docs.zip', name: 'docs'

                        recordIssues(
                            filters: [includeFile("${baseFolder}/.*")],
                            qualityGates: [[threshold: 1, type: 'DELTA', unstable: true]],
                            referenceJobName: "devtools/${baseFolder}/nightly",
                            tools: [doxygen(pattern: 'build.log')]
                        )
                    }
                    post { always { cleanWs() } }
                }
            }
        }

        stage('Installer') {
            agent { label 'Linux-MDK' }
            steps {
                // method definition outside pipeline{} block
                installerStep([
                    artifactory             : artifactory,
                    linBinFolder            : linBinFolder,
                    linBaseFolder           : linBaseFolder
                ])
            }
            post { always { cleanWs() } }
        }

        stage('Tests') {
            parallel {
                stage('Unit_Tests_Linux') {
                    agent {
                        docker {
                            image "${DOCKER_TAG}"
                            label "Linux"
                            registryUrl "https://${DOCKER_REGISTRY}"
                            registryCredentialsId "${DOCKER_REGISTRY_CREDENTIALS_ID}"
                            args "-v ${lin_packs}/:${lin_packs}/"
                            alwaysPull true
                        }
                    }
                    when { expression { 'linux' in systems } }
                    environment {
                        CI_CBUILD_INSTALLER="${WORKSPACE}/${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                        CI_PACK_ROOT = "${lin_packs}/${EXECUTOR_NUMBER}/"
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        script {
                            retry(3) {
                                artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                                sh("chmod 777 ${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh")
                            }
                        }
                        dir("build") {
                            sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..")
                            sh("cmake --build . --parallel 4 --target CbuildUnitTests")

                            sh("${linBaseFolder}/test/unittests/linux64/Debug/CbuildUnitTests --gtest_output=xml:test_report_unit_lin.xml")

                            archiveArtifacts 'test_report_unit_lin.xml'
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage('Unit_Tests_MacOS') {
                    agent { label 'macos'}
                    when { expression { 'macos' in systems } }
                    environment {
                        CI_PACK_ROOT             = "${mac_packs}/${EXECUTOR_NUMBER}/"
                        CI_ARMCC5_TOOLCHAIN_ROOT = ""
                        CI_ARMCC6_TOOLCHAIN_ROOT = "${ARMCC6_PATH}/bin"
                        CI_GCC_TOOLCHAIN_ROOT    = "${GCC_PATH}/bin"
                        CI_CBUILD_INSTALLER="${WORKSPACE}/${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        script {
                            retry(3) {
                                artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                                sh("chmod 777 ${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh")
                            }
                        }
                        dir("build") {
                            sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..")
                            sh("cmake --build . --parallel 4 --target CbuildUnitTests")

                            sh("${linBaseFolder}/test/unittests/darwin64/Debug/CbuildUnitTests --gtest_output=xml:test_report_unit_mac.xml")

                            archiveArtifacts 'test_report_unit_mac.xml'
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage('Unit_Tests_Windows_x64') {
                    agent { label 'Windows-MDK' }
                    environment {
                        CI_PACK_ROOT             = "${win_packs}\\${EXECUTOR_NUMBER}\\"
                        CI_ARMCC5_TOOLCHAIN_ROOT = "${ARMCC5_PATH}\\bin"
                        CI_ARMCC6_TOOLCHAIN_ROOT = "${ARMCC6_PATH}\\bin"
                        CI_GCC_TOOLCHAIN_ROOT    = "${GCC_PATH}\\bin"
                        // Windows bash friendly path
                        GIT_SSH_COMMAND          = "ssh -i /C/users/jenkins/.ssh/id_rsa"
                        // Bug workaround https://gitlab.com/tortoisegit/tortoisegit/-/issues/3139
                        GIT_SSH_VARIANT          = "ssh"
                        CI_CBUILD_INSTALLER      = "${WORKSPACE}\\${winBaseFolder}\\cbuildgen\\installer\\output\\cbuild_install.sh"
                    }
                    when { expression { 'win64' in systems } }
                    steps {
                        checkoutScmWithRetry(3)
                        script {
                            retry(3) {
                                artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                            }
                        }
                        dir("build") {
                            bat("""
                                call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat"
                                cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
                                if %errorlevel% neq 0 exit /b %errorlevel%
                                cmake --build . --parallel 4 --target CbuildUnitTests
                                exit /b %errorlevel%
                            """)
                            bat("${winBaseFolder}\\test\\unittests\\windows64\\Debug\\CbuildUnitTests.exe --gtest_output=xml:test_report_unit_win.xml")

                            xunit([GoogleTest(deleteOutputFiles: true,
                                failIfNotNew: true,
                                pattern: 'test_report_*.xml',
                                skipNoTestFiles: false,
                                stopProcessingIfError: true)])
                            archiveArtifacts 'test_report_unit_win.xml'
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage('Integration_Test_Linux') {
                    agent {
                        docker {
                            image "${DOCKER_TAG}"
                            label "Linux"
                            registryUrl "https://${DOCKER_REGISTRY}"
                            registryCredentialsId "${DOCKER_REGISTRY_CREDENTIALS_ID}"
                            args "-v ${lin_packs}/:${lin_packs}/"
                            alwaysPull true
                        }
                    }
                    when { expression { 'linux' in systems } }
                    environment {
                        CI_CBUILD_INSTALLER="${WORKSPACE}/${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                        CI_CBUILD_DEB_PKG="${WORKSPACE}/${linBaseFolder}/cbuildgen/installer/output/debian"
                        CI_PACK_ROOT="${lin_packs}/${EXECUTOR_NUMBER}/"
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        script {
                            retry(3) {
                                artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                                sh("chmod 777 ${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh")
                                artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/debian/cmsis-build*.deb"
                            }
                        }
                        dir("build") {
                            sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..")
                            sh("cmake --build . --parallel 4 --target CbuildIntegTests")
                            sh("${linBaseFolder}/test/integrationtests/linux64/Debug/CbuildIntegTests --gtest_output=xml:test_report_integration_lin.xml")

                            xunit([GoogleTest(deleteOutputFiles: true,
                                failIfNotNew: true,
                                pattern: 'test_report_*.xml',
                                skipNoTestFiles: false,
                                stopProcessingIfError: true)])
                            archiveArtifacts 'test_report_integration_lin.xml'
                        }

                    }
                    post { always { cleanWs() } }
                }

                stage('Integration_Test_MacOS') {
                    agent { label 'macos'}
                    when { expression { 'macos' in systems } }
                    environment {
                        CI_PACK_ROOT             = "${mac_packs}/${EXECUTOR_NUMBER}/"
                        CI_ARMCC5_TOOLCHAIN_ROOT = ""
                        CI_ARMCC6_TOOLCHAIN_ROOT = "${ARMCC6_PATH}/bin"
                        CI_GCC_TOOLCHAIN_ROOT    = "${GCC_PATH}/bin"
                        CI_CBUILD_INSTALLER="${WORKSPACE}/${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        script {
                            retry(3) {
                                artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                                sh("chmod 777 ${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh")
                            }
                        }
                        dir("build") {
                            sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..")
                            sh("cmake --build . --parallel 4 --target CbuildIntegTests")
                            sh("${linBaseFolder}/test/integrationtests/darwin64/Debug/CbuildIntegTests --gtest_filter=-*DebPkgTests*:*AC5* --gtest_output=xml:test_report_integration_mac.xml")

                            xunit([GoogleTest(deleteOutputFiles: true,
                                failIfNotNew: true,
                                pattern: 'test_report_*.xml',
                                skipNoTestFiles: false,
                                stopProcessingIfError: true)])
                            archiveArtifacts 'test_report_integration_mac.xml'
                        }

                    }
                    post { always { cleanWs() } }
                }

                stage('Integration_Tests_Windows_x64') {
                    agent { label 'Windows-MDK' }
                    when { expression { 'win64' in systems } }
                    environment {
                        CI_PACK_ROOT             = "${win_packs}\\${EXECUTOR_NUMBER}\\"
                        CI_ARMCC5_TOOLCHAIN_ROOT = "${ARMCC5_PATH}\\bin"
                        CI_ARMCC6_TOOLCHAIN_ROOT = "${ARMCC6_PATH}\\bin"
                        CI_GCC_TOOLCHAIN_ROOT    = "${GCC_PATH}\\bin"
                        // Windows bash friendly path
                        GIT_SSH_COMMAND          = "ssh -i /C/users/jenkins/.ssh/id_rsa"
                        // Bug workaround https://gitlab.com/tortoisegit/tortoisegit/-/issues/3139
                        GIT_SSH_VARIANT          = "ssh"
                        CI_CBUILD_INSTALLER      = "${WORKSPACE}\\${winBaseFolder}\\cbuildgen\\installer\\output\\cbuild_install.sh"
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        script {
                            retry(3) {
                                artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                            }
                        }
                        dir("build") {
                            bat("""
                                call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat"
                                cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
                                if %errorlevel% neq 0 exit /b %errorlevel%
                                cmake --build . --parallel 4 --target CbuildIntegTests
                                exit /b %errorlevel%
                            """)
                            bat("${winBaseFolder}\\test\\integrationtests\\windows64\\Debug\\CbuildIntegTests.exe --gtest_filter=-*DebPkgTests* --gtest_output=xml:test_report_integration_win.xml")

                            xunit([GoogleTest(deleteOutputFiles: true,
                                failIfNotNew: true,
                                pattern: 'test_report_*.xml',
                                skipNoTestFiles: false,
                                stopProcessingIfError: true)])
                            archiveArtifacts 'test_report_integration_win.xml'
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage('OTG CMSIS Acceptance Test') {
                    agent { label 'Linux' }
                    when { expression { return runOTGtests }}
                    // default values: https://github.com/ARMmbed/build-service/blob/master/.env.docker-compose-acceptancetests
                    environment {
                        AWS_ACCESS_KEY_ID           = credentials('OTG_AWS_ACCESS_KEY_ID')
                        AWS_DEFAULT_REGION          = 'us-west-2'
                        AWS_SECRET_ACCESS_KEY       = credentials('OTG_AWS_SECRET_ACCESS_KEY')
                        DOCKER_REGISTRY             = '104059736540.dkr.ecr.us-west-2.amazonaws.com'
                        KEIL_WEBSITE_IP             = '172.18.8.105'
                        TEST_AUTHMETHOD             = 'OAUTH2'
                        TEST_ACCESSTOKEN            = 'otg-dev'
                        TEST_DEBIAN                 = "${WORKSPACE}"
                        TEST_INPUT                  = "${WORKSPACE}/tools/buildmgr/test/testinput/Examples/AC6/"
                        TEST_PACKS                  = "${WORKSPACE}/packs"
                        TEST_PASSWORD               = 'otg-dev'
                        TEST_OUTPUT                 = "${WORKSPACE}/results"
                        TEST_USER                   = 'localdev'
                        TEST_USER_WORKSPACE         = '2/28380fab20fc08f01ca453073424d1e0'
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        script {
                            retry(3) {
                                artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/debian/cmsis-build*.deb"
                            }
                            try {
                                retry(2) {
                                    // retry twice, sometimes when the containers does not initiate properly.
                                    sh("run_acceptancetests")
                                }
                            } finally {
                                zip dir: "${WORKSPACE}/results", glob: '', zipFile: 'acceptance_tests_results.zip'
                                archiveArtifacts 'acceptance_tests_results.zip'
                                xunit([GoogleTest(deleteOutputFiles: false,
                                       failIfNotNew: true,
                                       pattern: 'results/output/report.xml',
                                       skipNoTestFiles: false,
                                       stopProcessingIfError: true)])
                            }
                        }
                    }
                    post { always { cleanWs() } }
                }

                stage('Code_Coverage') {
                    agent {
                        docker {
                            image "${DOCKER_TAG}"
                            label "Linux"
                            registryUrl "https://${DOCKER_REGISTRY}"
                            registryCredentialsId "${DOCKER_REGISTRY_CREDENTIALS_ID}"
                            args "-v ${lin_packs}/:${lin_packs}/"
                            alwaysPull true
                        }
                    }
                    when { expression { 'linux' in systems } }
                    environment { CI_PACK_ROOT = "${lin_packs}/${EXECUTOR_NUMBER}/" }
                    steps {
                        checkoutScmWithRetry(3)
                        script {
                            retry(3) {
                                artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                                sh("chmod 777 ${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh")
                            }
                        }
                        dir("build") {
                            sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON ..")

                            // Code Coverage Cobertura - Integration Tests
                            sh("cmake --build . --parallel 4 --target CbuildIntegTests")
                            sh("""${linBaseFolder}/test/integrationtests/linux64/Debug/CbuildIntegTests --gtest_filter=-*InstallerTests*:*DebPkgTests*""")
                            sh("gcovr -r ../${linBaseFolder} ${linBaseFolder}/cbuild ${linBaseFolder}/cbuildgen --exclude=RTE --exclude=XMLTree --json -o cobertura_Integ.json")

                            // Code Coverage Cobertura - Unit Tests
                            sh("cmake --build . --parallel 4 --target CbuildUnitTests")
                            sh("""${linBaseFolder}/test/unittests/linux64/Debug/CbuildUnitTests""")
                            sh("gcovr -r ../${linBaseFolder} ${linBaseFolder}/cbuild ${linBaseFolder}/cbuildgen --exclude=RTE --exclude=XMLTree --json -o cobertura_Unit.json")

                            // Merge coverage data
                            sh("gcovr --add-tracefile cobertura_Integ.json --add-tracefile cobertura_Unit.json --xml cobertura.xml")

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
                stage('Dynamic_Analysis') {
                    when { expression { return runDynamicAnalysis }}
                    agent {
                        docker {
                            image "${DOCKER_TAG}"
                            label "Linux"
                            registryUrl "https://${DOCKER_REGISTRY}"
                            registryCredentialsId "${DOCKER_REGISTRY_CREDENTIALS_ID}"
                            args "-v ${lin_packs}/:${lin_packs}/"
                            alwaysPull true
                        }
                    }
                    environment {
                        CI_CBUILD_INSTALLER="${WORKSPACE}/${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                        CI_PACK_ROOT="${lin_packs}/${EXECUTOR_NUMBER}/"
                    }
                    steps {
                        checkoutScmWithRetry(3)
                        script {
                            retry(3) {
                                artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                                sh("chmod 777 ${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh")
                            }
                        }
                        dir("build") {
                            sh("cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..")
                            sh("cmake --build . --parallel 4 --target CbuildIntegTests")
                            sh("valgrind -q --tool=memcheck --leak-check=full --trace-children=yes --trace-children-skip=*/tar,*/date,*/dos2unix,*/tail,*/rm,*/git,*/basename --xml=yes --xml-file=vg_dyn_analysis.%p.memcheck ${linBaseFolder}/test/integrationtests/linux64/Debug/CbuildIntegTests --gtest_filter=*CBuildGenTests*")
                        }

                        publishValgrind (
                          failBuildOnInvalidReports: false,
                          failBuildOnMissingReports: false,
                          failThresholdDefinitelyLost: '',
                          failThresholdInvalidReadWrite: '',
                          failThresholdTotal: '',
                          pattern: 'build/*.memcheck',
                          publishResultsForAbortedBuilds: false,
                          publishResultsForFailedBuilds: false,
                          sourceSubstitutionPaths: '',
                          unstableThresholdDefinitelyLost: '',
                          unstableThresholdInvalidReadWrite: '',
                          unstableThresholdTotal: ''
                        )
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
                            sh("cov-build --config cov_config_file --dir idir cmake --build . --parallel 4 --target cbuildgen")
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

        stage('Sending installers to the Last Successful Nightly Build location') {
            agent {
                docker {
                    image "${DOCKER_TAG}"
                    label "Linux"
                    registryUrl "https://${DOCKER_REGISTRY}"
                    registryCredentialsId "${DOCKER_REGISTRY_CREDENTIALS_ID}"
                    alwaysPull true
                }
            }
            // Important: only run this stage when the expression is satisfied.
            when { expression { return runSendLastNightly } }
            steps {
                script {
                    retry(3) {
                        artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_dist.zip"
                        artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh"
                        // download and rename it to have a fixed name in order to be overwritten when uploading to Artifactory
                        artifactory.download pattern: "${linBaseFolder}/cbuildgen/installer/output/debian/cmsis-build*.deb",
                                            target:  "${linBaseFolder}/cbuildgen/installer/output/debian/cmsis-build-install-nightly.deb"

                        artifactory.upload pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_dist.zip",
                                        target: "mcu.cmsis/ci/artifacts/${env.JENKINS_ENV}/devtools/${baseFolder}/${JOB_BASE_NAME}/lastSuccessful/cbuild_dist.zip"
                        artifactory.upload pattern: "${linBaseFolder}/cbuildgen/installer/output/cbuild_install.sh",
                                        target: "mcu.cmsis/ci/artifacts/${env.JENKINS_ENV}/devtools/${baseFolder}/${JOB_BASE_NAME}/lastSuccessful/cbuild_install.sh"
                        artifactory.upload pattern:  "${linBaseFolder}/cbuildgen/installer/output/debian/cmsis-build-install-nightly.deb",
                                        target: "mcu.cmsis/ci/artifacts/${env.JENKINS_ENV}/devtools/${baseFolder}/${JOB_BASE_NAME}/lastSuccessful/cmsis-build-install-nightly.deb"
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