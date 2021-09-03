<#
.SYNOPSIS
    Written by Eric - Summer 2021
        ###
    Runs a simulation on the inputed area (see -Example), while
    offering many options such as recompiling the simulator before running,
    cleaning specific simulation run folders, only running the scenario generation
    script, ...
.DESCRIPTION
    Running a simple simulation (see example 1) performs these steps:
        1. Generate scenario file from config file (Python script)
        2. Runs simulator for inputed number of days (default=500)
        3. Generates graphs per region in the area if the flag is set (default=off)
        4. Generates graphs for the area
        5. Parses data to be used by webviewer
        6. Clean ups

    This script is dependent on:
        - geopandas
        - cmake
        - matplotlib
        - numpy
        - java
        - gcc (g++)
        - Cadmium repo in the same directory as SEVIRDS
.INPUTS
    None. Once you set the flags you want, everything is handled for you
.EXAMPLE
    .\run_simulation.ps1 -Area on
        Runs a simulation on the Ontario config
.EXAMPLE
    .\run_simulation.ps1 -Area on -Rebuild
        Re-compiles the simulator then runs a simulation on the Ontario config
.EXAMPLE
    .\run_simulation.ps1 -GenScenario -Area on
        Generates the scenario file used by the simulator using the Ontario config. Running a sim
        like in examples 1 and 2 does this automatically and this is for when you just want the
        scenario file re-done (useful when debugging)
.EXAMPLE
    .\run_simulation.ps1 -GraphRegion -Area on
        Runs a simulation on the Ontario config and generates graphs per region since
        by default this is turned off (it takes a longer time to do then Aggregated graphs)
        and isn't always that useful)
.Link
    https://github.com/SimulationEverywhere-Models/Geography-Based-SEIRDS-Vaccinated
#>

[CmdletBinding()]
param(
    # Sets the Config to run a simulation on (Currently supports Ontario and Ottawa
    [string]$Config = "",

    # Cleans the simulation run with the inputed folder name such as '-Clean run1' (Needs to set the Config).
    # Setting the run name to _ cleans all the runs under the set config
    # Ex: -Clean _ ontario
    #   Will clean all runs under GIS_Viewer\ontario\
    [string]$Clean = "",

    # Sets the number of days to run a simulation (default=500)
    [int32]$Days = 500,

    # Generates a scenario json file (an area flag needs to be set)
    [switch]$GenScenario = $False,

    # Will generate graphs per region after running the simulation (default=off)
    [switch]$GraphPerRegions = $False,

    # Generates graphs per region for the specified run
    [string]$GenRegionGraphs = "",

    # Sets the name of the folder to save the run
    [string]$Name = "",

    # Turns off progress and loading animations
    [switch]$NoProgress = $False,

    # Re-Compiles the simulator
    [switch]$Rebuild = $False,

    # Fully Re-Compiles the simulator (i.e., rebuilds the CMake and Make files and cache)
    [switch]$FullRebuild = $False,

    # Builds a debug version of the siomulator
    [switch]$DebugSim = $False,

    # Set to true to create a zipped simulator package
    [switch]$Export = $False
) #params()

# Check if any of the above params were set
$private:Params        = "Config", "Clean", "Days", "GenScenario", "GraphPerRegions", "GenRegionGraphs", "Name", "NoProgress", "Rebuild", "FullRebuild", "DebugSim", "Export"
$private:ParamsNotNull = $False
foreach($Param in $Params) { if ($PSBoundParameters.keys -like "*"+$Param+"*") { $ParamsNotNull = $True; break; } }

# <Colors> #
    $RESET  = "[0m"
    $RED    = "[31m"
    $GREEN  = "[32m"
    $YELLOW = "[33m"
    $BLUE   = "[36m"
    $BOLD   = "[1m"
# </Colors> #

# <Helpers> #
    <#
        .SYNOPSIS
        Handles quitting the scripts and setting the correct location in the terminla
        .PARAMETER code
        Error code to exit with
    #>
    function Quit([int] $private:code=0) {
        Set-Location $InvokeDir
        exit $code
    }

    <#
        .SYNOPSIS
        Checks if any errors have been returned and stops scripts if so.
        Should be placed directly after a program call.
        .EXAMPLE
        python generate_scenario.py
        ErrorCheck
            If the generate_scenario returns an error the script will exit.
    #>
    function ErrorCheck() {
        # 0 => All is good
        if ($LASTEXITCODE -ne 0) {
            ComputeBuildTime $False $Stopwatch # Display failed message and time
            Quit($LASTEXITCODE)
        }
    }

    <#
        .SYNOPSIS
        Generates the scenario file for a region and places it in the config directory
        .PARAMETER Private:Config
        Config to generate scenario. Case sensitive
        .EXAMPLE
        GenerateScenario ontario
    #>
    function GenerateScenario([string] $private:Config)
    {
        # Create output directory if non-existant
        if (!(Test-Path ".\Scripts\Input_Generator\output")) { New-Item ".\Scripts\Input_Generator\output" -ItemType Directory | Out-Null }

        Write-Output "Generating $BLUE$Config$RESET Scenario:"
        Set-Location .\Scripts\Input_Generator

        python.exe generateScenario.py $Config $PROGRESS
        ErrorCheck

        Move-Item   .\output\scenario_${Config}.json ..\..\config -Force
        Remove-Item .\output -Recurse
        Set-Location $HomeDir
    }

    <#
        .SYNOPSIS
        Cleans either all runs or a specified run
        .PARAMETER private:VisualizationDir
        Path to the simulations runs
        .PARAMETER private:Run
        Name of specific run to clean
        .EXAMPLE
        Clean "GIS_Viewer/ontario/simulation_runs/" "run1"
            Cleans run1 found in "GIS_Viewer/ontario/simulation_runs/"
    #>
    function Clean([string] $private:VisualizationDir, [string] $private:Run)
    {
        [string] $private:Dir=$VisualizationDir
        [string] $private:RunName="all$RESET runs"

        if ($Run -ne "_") { $Dir="$Dir$Run";  $RunName="$Run$RESET" }

        Write-Output "Removing ${YELLOW}${RunName} from the ${YELLOW}${Config}${RESET} config"
        if (Test-Path $Dir) {
            Remove-Item $Dir -Recurse -Verbose 4>&1 |
            ForEach-Object{ `Write-Host ($_.Message -replace'(.*)Target "(.*)"(.*)',' $2') -ForegroundColor Red}
        }
        Write-Output ${GREEN}"Done."${RESET}
    }

    <#
        .SYNOPSIS
        Computes and diplas whether the build was a success
        and how much time is took using the the stopwatch
        .PARAMETER Success
        True if the simulation was a success
        .PARAMETER private:StopWatch
        Stopwartch object containing how much time has passed
        .EXAMPLE
        ComputeBuildTime $True $null
            Displays success message but no execution time
    #>
    function ComputeBuildTime([bool] $Success, [System.Diagnostics.Stopwatch] $private:StopWatch=$null)
    {
        if ($StopWatch)
        {
            $Hours   = $StopWatch.Elapsed.Hours
            $Minutes = $StopWatch.Elapsed.Minutes
            $Seconds = $StopWatch.Elapsed.Seconds
            $StopWatch.Stop()
        }

        $Color = (($Success) ? $GREEN : $RED)

        if ($Success) { Write-Host -NoNewline "`n${GREEN}Simulation Completed" }
        else          { Write-Host -NoNewline "`n${RED}Simulation Failed"      }

        if ($StopWatch)
        {
            Write-Host -NoNewline $Color" ("
            if ( $Hours -gt 0)    { Write-Host -NoNewline "${Color}${Hours}h"   }
            if ( $Minutes -gt 0 ) { Write-Host -NoNewline "${Color}${Minutes}m" }
            Write-Output "${Color}${Seconds}s)${RESET}"
        }
    }

    <#
        .SYNOPSIS
        Verifies ALL dependencies are met
        .PARAMETER private:Simulator
        Setting to true checks for dependencies related to compiling the simulator
        .PARAMETER private:Python
        Setting to true checks for dependencies related to running the python scripts
    #>
    function DependencyCheck([bool] $private:Simulator=$False, [bool] $private:Python=$False)
    {
        if ($Simulator -or $Python) {
            # Don't need to check again in the current shell
            if (($SimCheck -and $PyCheck) -or 
                (!$Simulator -and $PyCheck) -or
                (!$Python -and $SimCheck)) {
                Write-Verbose "${GREEN}Dependencies already met`n${RESET}"
                return 
            }

            Write-Verbose "Checking Dependencies..."

            # Setup dependency specific data
            $private:Dependencies = [Ordered]@{
                # dependency=version, website
                cmake="cmake version 3","https://cmake.org/download/";
                gcc="x86_64-posix-seh-rev0", "http://mingw-w64.org/doku.php";
            }

            # Python Depedencies
            $private:Libs = "numpy", "geopandas", "matplotlib"

            try {
                if ($Simulator -and !$SimCheck) {
                    # Test Cadmium
                    if ( !(Test-Path "../cadmium") )
                    {
                        $Parent = Split-Path -Path $HomeDir -Parent
                        Write-Verbose "Cadmium ${RED}[NOT FOUND]" -Verbose
                        Write-Verbose ${YELLOW}"Make sure it's in "${YELLOW}${Parent}
                        throw 1
                    } else { Write-Verbose "Cadmium ${GREEN}[FOUND]" }

                    # Loop through each dependency
                    foreach($private:Depends in $Dependencies.keys) {
                        # Try and get the version
                        $private:Version = cmd /c "$Depends --version"

                        # If the version is incorrect or non-existant, throw an error
                        if ( !($Version -clike "*"+${Dependencies}.${Depends}[0]+"*") ) { throw 1 }

                        # It was found
                        Write-Verbose "$Depends ${GREEN}[FOUND]"
                    }

                    $private:boost = $env:path | Select-String boost
                    if (!$boost) {
                        $Depends="Boost"
                        throw 2
                    } else { Write-Verbose "Boost ${GREEN}[FOUND]${RESET}" }

                    $Global:SimCheck = $True
                }

                if ($Python -and !$PyCheck) {
                    $private:Dependencies = [Ordered]@{
                        # dependency = min version, website
                        python = "Python 3", "https://www.python.org/downloads/";
                        conda  = "conda 4", "https://www.anaconda.com/products/individual#windows"
                    }

                    foreach ($private:Depends in $Dependencies.keys) {
                        # Try and get the version
                        $private:Version = cmd /c "$Depends --version"

                        # If the version is incorrect or non-existant, throw an error
                        if ( !($Version -clike "*" + ${Dependencies}.${Depends}[0] + "*") ) { throw 1 }

                        # It was found
                        Write-Verbose "$Depends ${GREEN}[FOUND]"
                    }

                    # Loop through each python library dependency
                    $private:CondaList = conda list
                    foreach($private:Depends in $Libs) {
                        if ( !($CondaList -like "*${Depends}*") ) { throw 3 }
                        Write-Verbose "$Depends ${GREEN}[FOUND]"
                    }

                    $Global:PyCheck = $True
                }

                Write-Verbose $GREEN"Completed Dependency Check.`n"$RESET
            } catch {
                Write-Verbose "$Depends ${RED}[NOT FOUND]" -Verbose

                # Dependency Prints
                if ($Error[0].Exception.Message -eq 1) {
                    $private:MinVersion = $Dependencies.$Depends[0]
                    Write-Verbose $YELLOW"Check that '$Depends --version' contains this version $MinVersion"
                    $private:Website = $Dependencies.$Depends[1]
                    Write-Verbose $YELLOW"$Depends for Windows can be installed from here: ${BLUE}$Website"

                    if ($Depends -eq "python" -or $Depends -eq "conda") { $PyCheck = $False }
                    else { $Global:SimCheck = $False }
                } elseif($Error[0].Exception.Message -eq 2) {
                    Write-Verbose $YELLOW"Check the Wiki for installation help: ${BLUE}https://github.com/SimulationEverywhere-Models/Geography-Based-SEIRDS-Vaccinated/wiki/Windows-10-%5C-11-(x64)#11-boost"
                # Python Dependency Prints
                } elseif($Error[0].Exception.Message -eq 3) {
                    Write-Verbose $YELLOW"Verify the correct conda environment is set"
                    Write-Verbose $YELLOW'Check `conda list  | Select-String "'"$Depends"'"`'
                    Write-Verbose $YELLOW'It can be installed using `conda install '$Depends'`'
                    $Global:PyCheck = $False
                } else{
                    Write-Error $Error[0].Exception.Message
                    $Global:SimCheck = $False
                    $Global:PyCheck  = $False
                }
            }

            if (($Simulator -and !$SimCheck) -or ($Python -and !$PyCheck)) { Quit(-1) }
        }
    } #DependencyCheck()

    <#
        .SYNOPSIS
        Builds the simulator if it is not and can also be set to rebuild it. Can also build Debug if $BuildFolder set correctly
        .PARAMETER private:Rebuild
        'True', rebuilds the simulator
        .PARAMETER private:FullRebuild
        'True', rebuilds the CMake cache and the simulator
        .PARAMETER private:BuildType
        The type of build (e.g., Release or Debug)
        .PARAMETER private:Verbose
        True prints out all warnings while building and completely
        rebuilds the simulator if Rebuild is set to true
        .EXAMPLE
        BuiSimulator $True "Debug" "Y" "Y"
            Completely rebuilds the simulator and creates a debug executable
    #>
    function BuildSimulator([bool] $private:Rebuild=$False, [bool] $private:FullRebuild=$False, [string] $private:BuildType="Release", [string] $private:Verbose="N")
    {
        # Remove the current executable
        if ($Rebuild -or $FullRebuild) {
            # Clean everything for a complete rebuild
            if ($FullRebuild -and (Test-Path ".\bin")) {
                Remove-Item .\bin -Recurse
            # Otherwise just clean the executable for a quick rebuild
            } elseif ( (Test-Path ".\bin\pandemic-geographical_model.exe") ) {
                Remove-Item .\bin\pandemic-geographical_model.exe
            }
        }

        # Build the executable if it doesn't exist
        if ( !(Test-Path ".\bin\pandemic-geographical_model.exe") ) {
            DependencyCheck $True

            Write-Verbose "${YELLOW}Building Model as $BLUE$BuildType${YELLOW}"
            cmake -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=$BuildType -DVERBOSE=$Verbose -B"${HomeDir}\bin" -G "MinGW Makefiles"
            ErrorCheck
        }

        cmake --build .\bin
        ErrorCheck
        Write-Verbose "${GREEN}Done.`n"
    }

    <#
        .SYNOPSIS
        Generates all the graphs for a single run, provided the correct variables are set
        .PARAMETER private:LogFolder
        Path to the log folder (can be relative to the script's directory)
        .PARAMETER private:GenAggregate
        True is the aggregated graphs should be generated
        .PARAMETER private:GenRegions
        True if the regional graphs should be generated
        .EXAMPLE
        GenerateGraphs "logs" $True $False
            Will generate the aggregated graphs using the data in the Geographical-Based-SEIRDS-Vaccinated/logs folder
    #>
    function GenerateGraphs([string] $private:LogFolder="", [bool] $private:GenAggregate=$True, [bool] $private:GenRegions=$False)
    {
        Write-Output "Generating Graphs:"
        $GenFolder = ".\Scripts\Graph_Generator"

        if ($LogFolder -eq "") {
            if (Test-Path "logs\stats") { Remove-Item logs/stats -Recurse }
            New-Item logs\stats -ItemType Directory | Out-Null
            $LogFolder = ".\logs"
        }

        if ($GenRegions) {
            python ${GenFolder}\graph_per_regions.py $Progress "-ld=$LogFolder"
            ErrorCheck
        }

        if ($GenAggregate) {
            python ${GenFolder}\graph_aggregates.py $Progress "-ld=$LogFolder"
        }
    }

    <#
        .SYNOPSIS
        Creates a .zip file with all the required files for just running simulations (no compiling required)
        and is uploaded to the releases page on Git: https://github.com/SimulationEverywhere-Models/Geography-Based-SEIRDS-Vaccinated/releases
    #>
    function Export()
    {
        Write-Verbose "${YELLOW}Exporting...${RESET}"

        # Remove pre-existing folders
        if (Test-Path ".\Out\Windows\Scripts")     { Remove-Item ".\Out\Windows\Scripts\"     -Recurse }
        if (Test-Path ".\Out\Windows\cadmium_gis") { Remove-Item ".\Out\Windows\cadmium_gis\" -Recurse }
        if (Test-Path ".\Out\Windows\bin")         { Remove-Item ".\Out\Windows\bin\"         -Recurse }
        if (Test-Path ".\Out\Windows\Results")     { Remove-Item ".\Out\Windows\Results\"     -Recurse }

        # Setup the folder with allt thre required files/folders
        New-Item    ".\Out\Windows\bin" -ItemType Directory | Out-Null
        Copy-Item   ".\bin\pandemic-geographical_model.exe" ".\Out\Windows\bin\"
        Copy-Item   ".\cadmium_gis\" ".\Out\Windows\" -Recurse
        Copy-Item   ".\Scripts\" ".\Out\Windows\" -Recurse
        Remove-Item ".\Out\Windows\Scripts\.gitignore"

        # Compress it to a .zip
        Compress-Archive .\Out\Windows -DestinationPath .\Out\SEVIRDS-Windowsx64.zip -Update

        # Clean setup folder
        Remove-Item ".\Out\Windows\Scripts\"     -Recurse
        Remove-Item ".\Out\Windows\cadmium_gis\" -Recurse
        Remove-Item ".\Out\Windows\bin\"         -Recurse
        if (Test-Path ".\Out\Windows\Results") { Remove-Item ".\Out\Windows\Results\" -Recurse }

        Write-Verbose "${GREEN}Done.${RESET}"
    }
# </Helpers> #

function Main()
{
    # Used for execution time at the end of Main()
    $Local:Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    # Create out directory
    if ($Name -ne "") { $VisualizationDir=$VisualizationDir+$Name }
    else {
        $i = 1
        while ($True) {
            if ( !(Test-Path "${VisualizationDir}run${i}") ) {
                $VisualizationDir=$VisualizationDir+"run"+$i
                break
            }
            $i++
        }
    }

    if ( !(Test-Path "logs")            ) { New-Item "logs" -ItemType Directory | Out-Null            }
    if ( !(Test-Path $VisualizationDir) ) { New-Item $VisualizationDir -ItemType Directory | Out-Null }

    # Generate Scenario file
    GenerateScenario $Config

    # Run simulation
    Set-Location bin
    Write-Output "`nExecuting model for $Days days:"
    .\pandemic-geographical_model.exe ..\config\scenario_${Config}.json $Days $Progress
    ErrorCheck
    Set-Location $HomeDir
    Write-Output "" # Print new line

    # Generate SEVIRDS graphs
    GenerateGraphs "" $True $GraphPerRegions

    # Currently the GIS Viewer is dependent on Java
    # But this will change at some point
    try { $private:Version = java --version }
    catch { $Version = "" }
    if ( ($Version -clike "*java 16*") ) {
        if ( !(Test-Path .\Scripts\Msg_Log_Parser\input)  ) { New-Item .\Scripts\Msg_Log_Parser\input  -ItemType Directory | Out-Null }
        if ( !(Test-Path .\Scripts\Msg_Log_Parser\output) ) { New-Item .\Scripts\Msg_Log_Parser\output -ItemType Directory | Out-Null }
        Copy-Item config\scenario_${Config}.json .\Scripts\Msg_Log_Parser\input
        Copy-Item .\logs\pandemic_messages.txt .\Scripts\Msg_Log_Parser\input

        # Run message log parser
        Write-Output "`nRunning message log parser"
        Set-Location .\Scripts\Msg_Log_Parser
        java -jar sim.converter.glenn.jar "input" "output"
        Expand-Archive -LiteralPath output\pandemic_messages.zip -DestinationPath output
        Set-Location $HomeDir

        Move-Item   .\Scripts\Msg_Log_Parser\output\messages.log   $VisualizationDir
        Move-Item   .\Scripts\Msg_Log_Parser\output\structure.json $VisualizationDir
        Remove-Item .\Scripts\Msg_Log_Parser\input  -Recurse
        Remove-Item .\Scripts\Msg_Log_Parser\output -Recurse
        Remove-Item .\Scripts\Msg_Log_Parser\*.zip
        Write-Output "${GREEN}Done."
    }

    Copy-Item .\cadmium_gis\${Area}\${Area}.geojson $VisualizationDir
    Copy-Item .\cadmium_gis\${Area}\visualization.json $VisualizationDir
    Move-Item logs $VisualizationDir

    ComputeBuildTime $True $Stopwatch

    Write-Host -NoNewline "View results using the files in ${BOLD}${BLUE}${VisualizationDir}${RESET}"
    if ( ($Version -clike "*java 16*") ) {
        Write-Host " and through the web viewer: ${BOLD}${BLUE}http://206.12.94.204:8080/arslab-web/1.3/app-gis-v2/index.html${RESET}"
    } else { Write-Host "" }
    Quit
}

if ($ParamsNotNull) {
    # Setup global variables
    $Script:Progress  = (($NoProgress) ? "-np" : "")
    $local:BuildType  = (($DebugSim) ? "Debug" : "Release")
    $local:Verbose    = (($VerbosePreference -eq "SilentlyContinue" ? "N" : "Y"))
    $Script:InvokeDir = Get-Location | Select-Object -ExpandProperty Path
    $Script:HomeDir   = Split-Path -Parent $Script:MyInvocation.MyCommand.Path

    Set-Location $HomeDir

    # Setup Config variables
    if (Test-Path ".\Scripts\Input_Generator\${Config}") {
        $VisualizationDir = ".\GIS_Viewer\${Config}\"

        # Retrieve the area from the config variable
        # Ex: ontario_booster2 -> $Area = 'ontario'
        $Area = $Config.Split("_")[0]
    } else {
        Write-Output "${RED}Could not find ${BOLD}'${Config}'${RESET}${RED}. Check the spelling and verify that the directory is under ${YELLOW}'Scripts\Input_Generator\'${RESET}"
        Quit(-1)
    }

    # If clean then do this before the dependency check
    # so it's quicker and we don't have to worry about having
    # things installed like Python
    if ($Clean -ne "") {
        if ($Config -eq "") { Write-Output "${RED}Config must be set${RESET}"; Quit(-1) }
        Clean $VisualizationDir $Clean
        Quit(-1)
    }

    # Only generate the scenario
    if ($GenScenario) {
        # A region must be set
        if ($Config -eq "") { Write-Output "${RED}Config must be set${RESET}"; Quit(-1) }
        DependencyCheck $False $True
        GenerateScenario $Config
    # Only generate the graphs per region on a specified run
    } elseif ($GenRegionGraphs) {
        # A region must be set
        if ($Config -eq "") { Write-Output "${RED}Config must be set${RESET}"; Quit(-1) }
        if ( !(Test-Path "${VisualizationDir}${GenRegionGraphs}") ) { Write-Output "${RED}${BOLD}${GenRegionGraphs}${RESET}${RED} does not exist!${RESET}"; Quit(-1) }
        DependencyCheck $False $True
        GenerateGraphs  "${VisualizationDir}${GenRegionGraphs}\logs" $False $True
    } elseif ($Export) { Export }
    else {
        BuildSimulator $Rebuild $FullRebuild $BuildType $Verbose

        if ($Config -ne "") {
            DependencyCheck $False $True
            Main
        }
    }
}
# Display the help if no params were set
else {
    if ($VerbosePreference) { Get-Help .\run_simulation.ps1 -full }
    else { Get-Help .\run_simulation.ps1 }
}