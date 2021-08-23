[CmdletBinding()]
param(
    [string]$Area = "",

    [string]$Days = 500,

    [switch]$NoProgress = $False,

    [switch]$GraphPerRegions = $False,

    [switch]$GenerateGraphsPerRegions = $False,

    [switch]$DirName = ""
)

# <Colors> #
    $RESET  = "[0m"
    $RED    = "[31m"
    $GREEN  = "[32m"
    $YELLOW = "[33m"
    $BLUE   = "[36m"
    $BOLD   = "[1m"
# </Colors> #


$private:Params = "Area", "Days", "NoProgress", "GraphPerRegions", "GenerateGraphsPerRegions", "DirName"
$private:ParamsNotNull = $False
foreach ($Param in $Params) { if ($PSBoundParameters.keys -like "*" + $Param + "*") { $ParamsNotNull = $True; break; } }

if ($ParamsNotNull) {
    # Setup global variables
    $Script:Progress = (($NoProgress) ? "-np" : "")
    $local:Verbose = (($VerbosePreference -eq "SilentlyContinue" ? "N" : "Y"))
    $Script:HomeDir = [System.Environment]::CurrentDirectory

    if ($Area -ne "") {
        if ($Area -eq "ontario" -or $Area -eq "on") {
            $Area = "ontario"
        }
        elseif ($Area -eq "ottawa" -or $Area -eq "ot") {
            $Area = "ottawa"
        }
        else {
            Write-Output "${RED}Unknown Area ${BOLD}${Area}${RESET}"
            exit -1
        }

        $VisualizationDir = "Results/${Area}/"
    }

    # DependencyCheck

    # Used for execution time at the end of Main 
    $Local:Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    if ($Name -ne "") { $VisualizationDir = $VisualizationDir + $Name }
    else {
        $i = 1
        while ($True) {
            if ( !(Test-Path "${VisualizationDir}run${i}") ) {
                $VisualizationDir = $VisualizationDir + "run" + $i
                break
            }
            $i++
        }
    }

    if ( !(Test-Path "logs") ) { New-Item "logs" -ItemType Directory | Out-Null }
    if ( !(Test-Path $VisualizationDir) ) { New-Item $VisualizationDir -ItemType Directory | Out-Null }

    # GenerateScenario $Area

    Set-Location bin
    Write-Output "`nExecuting model for $Days days:"
    .\pandemic-geographical_model.exe ../Scripts/Input_Generator/output/scenario_${Area}.json $Days $Progress
    ErrorCheck
    Set-Location $HomeDir
    Write-Output "" # New line

    # Generate SEVIRDS graphs
    # GenerateGraphs "" $True $GraphPerRegions

    if ( !(Test-Path .\Scripts\Msg_Log_Parser\input) ) { New-Item .\Scripts\Msg_Log_Parser\input  -ItemType Directory | Out-Null }
    if ( !(Test-Path .\Scripts\Msg_Log_Parser\output) ) { New-Item .\Scripts\Msg_Log_Parser\output -ItemType Directory | Out-Null }
    Copy-Item .\Scripts\Input_Generator\output\scenario_${Area}.json .\Scripts\Msg_Log_Parser\input
    Copy-Item .\logs\pandemic_messages.txt .\Scripts\Msg_Log_Parser\input

    # Run message log parser
    Write-Output "`nRunning message log parser"
    Set-Location .\Scripts\Msg_Log_Parser
    java -jar sim.converter.glenn.jar "input" "output"
    Expand-Archive -LiteralPath output\pandemic_messages.zip -DestinationPath output
    Set-Location $HomeDir

    Move-Item .\Scripts\Msg_Log_Parser\output\messages.log $VisualizationDir
    Move-Item .\Scripts\Msg_Log_Parser\output\structure.json $VisualizationDir
    Remove-Item .\Scripts\Msg_Log_Parser\input -Recurse
    Remove-Item .\Scripts\Msg_Log_Parser\output -Recurse
    Remove-Item .\Scripts\Msg_Log_Parser/*.zip
    Write-Output "${GREEN}Done."

    Copy-Item .\cadmium_gis\${Area}\${Area}.geojson $VisualizationDir
    Copy-Item .\GIS_Viewer\${Area}\visualization.json $VisualizationDir
    Move-Item logs $VisualizationDir


}
