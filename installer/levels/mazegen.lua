-------------------------------------------------------------------------------
-- LevelGen script creates a random maze
-- Args: gridXSize, gridYSize, ulCornerX, ulCornerY, cellsize
--
--    gridXSize, gridYSize --> Number of cells in the maze (defaults to 10, 10)
--    ulCornerX, ulCornerY --> Coordinates of upper left corner of maze
--                        (defaults to 0, 0)
--      cellsize --> Size of each maze cell (defaults to 1)
--
-------------------------------------------------------------------------------



function isUnvisited(cell)
  return cell["N Wall"] and cell["S Wall"] and cell["E Wall"] and cell["W Wall"]
end


function getAdjacentUnvisitedCells(cell)

  local x = cell["x"]
  local y = cell["y"]

  adjCells = {}
  adjDirs = {}

  if(not cells[x][y]["N Edge"] and isUnvisited(cells[x][y - 1])) then
     table.insert(adjCells, cells[x][y - 1])
     table.insert(adjDirs, "N")
  end

  if(not cells[x][y]["S Edge"] and isUnvisited(cells[x][y + 1])) then
     table.insert(adjCells, cells[x][y + 1])
     table.insert(adjDirs, "S")
  end

  if(not cells[x][y]["E Edge"] and isUnvisited(cells[x + 1][y])) then
     table.insert(adjCells, cells[x + 1][y])
     table.insert(adjDirs, "E")
  end

  if(not cells[x][y]["W Edge"] and isUnvisited(cells[x - 1][y])) then
     table.insert(adjCells, cells[x - 1][y])
     table.insert(adjDirs, "W")
  end
end


-- Knock down wall to neighbor cell, and neighbor's wall to current cell
function knockDownWallToCell(cell, dir, adjacentCell)
  cell[dir .. " Wall"] = false
  adjacentCell[oppDir(dir) .. " Wall"] = false
end


function oppDir(dir)
  if(dir == "N") then return "S" end
  if(dir == "S") then return "N" end
  if(dir == "E") then return "W" end
  if(dir == "W") then return "E" end
end


function initGridCells(xsize, ysize)
  for x = 0, xsize + 1 do    -- Add extra cell to avoid bounds checking later
     cells[x] = {}
     for y = 0, ysize + 1 do
        cells[x][y] = {}

        cells[x][y]["N Wall"] = true
        cells[x][y]["S Wall"] = true
        cells[x][y]["E Wall"] = true
        cells[x][y]["W Wall"] = true

        cells[x][y]["x"] = x
        cells[x][y]["y"] = y

        cells[x][y]["N Edge"] = (y == 1)
        cells[x][y]["S Edge"] = (y == gridYSize)
        cells[x][y]["E Edge"] = (x == gridXSize)
        cells[x][y]["W Edge"] = (x == 1)
     end
  end
end


function outputMaze()

   -- We'll output our maze as a series of tiles, each being 2 x cellsize square
   -- Divide each tile into quadrants, as follows:
   --           X----+----+
   --           | UL | UR |    * denotes point (cenx, ceny)
   --           |    |    |    size of each cell is (gridXSize, gridXSize)
   --           +----*----+    coords of point X on upper left cell are
   --           | LL | LR |       (ulCornerX, ulCornerY)
   --           |    |    |    UL is always filled in, LR is always open
   --           +----+----+    LL is filled in when an E wall is present
   --                          UR is filled in when a N wall is present
   --  When a series of these tiles are tiled together, a maze will be drawn
   --  with an open bottom and open right side, which will be closed at the end.
   --
   --  Wall segments are drawn as 2-pt horizontal lines with the appropriate
   --  thickness to make them square.  Adjacent wall segments are partially
   --  merged into longer horizontal lines to reduce vertices and therefore
   --  level transfer size.  Note we're taking advantage of Bitfighter's vertex
   --  thinning algorithms to make our job just a bit easier.

   local gridsize = levelgen:getGridSize() + 1 -- Add 1 to get a tiny bit of
                                               -- overlap to make walls merge
   local wallsize = gridsize * cellsize
   local points
   local vwallstart = { }
   local vwallend = { }

   for y = 1, gridYSize do

      local xstart = nil

      for x = 1, gridXSize do
         local cenx = ulCornerX + (2 * x - 1) * cellsize
         local ceny = ulCornerY + (2 * y - 1) * cellsize
         local cell = cells[x][y]


         -- UL quadrant is always drawn
         if(xstart == nil) then        -- No line in progress
            xstart = cenx - cellsize
         end

         -- UR quadrant drawn only when a N wall is present
         local endx

         if(cell["N Wall"]) then
           endx = cenx + cellsize
         else
           endx = cenx
         end


         if(not cell["N Wall"] or cell["E Edge"]) then

            -- Skip a horizontal wall if it's only one cell wide, because
            -- that portion will get covered by a vertical segment later.
            if(endx - xstart > cellsize) then
               local pts = { Point(xstart, ceny - cellsize * 0.5), Point(endx, ceny - cellsize * 0.5) }
               levelgen:addWall(wallsize, false, pts)
            end

            xstart = nil
         end

         -- LL quadrant drawn only when a W wall is present
         if(cell["W Wall"]) then
            if vwallstart[x] == nil then
               vwallstart[x] = Point(cenx - cellsize * 0.5, ceny - cellsize )
            end

            vwallend[x] = Point(cenx - cellsize * 0.5, ceny + cellsize * 2)

         else  -- Finish vertical wall
            if(vwallstart[x]) then
               local pts = { vwallstart[x], vwallend[x] }

               levelgen:addWall(wallsize, false, pts)
               vwallstart[x] = nil
            end
         end

         -- LR quadrant is always open, so do nothing
      end

   end

   -- Now add walls along entire left, bottom and right sides of the maze
   local pts = { Point(ulCornerX  + cellsize * 0.5,
                       ulCornerY),
                 Point(ulCornerX  + cellsize * 0.5,
                       ulCornerY + 2 * gridYSize * cellsize + cellsize * 0.5),
                 Point(ulCornerX + 2 * gridXSize * cellsize + cellsize * 0.5,
                       ulCornerY + 2 * gridYSize * cellsize + cellsize * 0.5),
                 Point(ulCornerX + 2 * gridXSize * cellsize + cellsize * 0.5,
                       ulCornerY) }
   levelgen:addWall(wallsize, false, pts)
end



-----------------------------------------------------------
-----------------------------------------------------------
-----------------------------------------------------------
-----------------------------------------------------------
-----------------------------------------------------------
-----------------------------------------------------------
-----------------------------------------------------------
-----------------------------------------------------------
-----------------------------------------------------------


-- Global variables

-- Remember, in Bitfighter, all global variables must be
-- defined in the main block!

gridXSize = tonumber(arg[1])
gridYSize = tonumber(arg[2])
ulCornerX = tonumber(arg[3])
ulCornerY = tonumber(arg[4])
cellsize = tonumber(arg[5])

if gridXSize == nil then gridXSize = 10 end
if gridYSize == nil then gridYSize = 10 end
if ulCornerX == nil then ulCornerX = 0 end
if ulCornerY == nil then ulCornerY = 0 end
if cellsize == nil then cellsize = 1 end


cellStack = List:new()

cells = {}
adjCells = {}
adjDirs = {}

-- Initialize cells
initGridCells(gridXSize, gridYSize)



-- Get starting cell
local x = math.random(gridXSize)
local y = math.random(gridYSize)

local currCell = cells[x][y]
local totalCells = gridXSize * gridYSize

local visitedCells = 1

while visitedCells < totalCells do

  -- Find all adjacent cells with all walls intact (sets adjCells and adjDirs)
  getAdjacentUnvisitedCells(currCell)
  if(#adjCells > 0) then                   -- More than one?
     local adj = math.random(#adjCells)    -- Pick one at random
          knockDownWallToCell(currCell, adjDirs[adj], adjCells[adj])

          -- Push current cell location on the cellStack; we'll revisit it later
          cellStack:pushright(currCell)

          -- Make the randomly selected adjacent cell the new current cell
          currCell = adjCells[adj]

     visitedCells = visitedCells + 1
  else
     -- This cell has no neighbors, so let's grab one from the stack
     currCell = cellStack:popright()
  end
end

-- Output the results
outputMaze()
