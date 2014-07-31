% Test of staggered gridding and null spaces in 2D.
% This show HOW application of no-slip boundary condition makes pressure
% variable coefficients vanish from momentum equations at grid points
% corresponding to south wall(x,1), west wall(1,y) and top-right
% corner(xn,yn). This means to these grid points we cannot apply continuity
% equation since it does not have p. Thus we need equation involving p for
% these grid points. The choices are: Dirchlet p and Neumann p. We explore
% these possibilities here.
% Regular staggered grid using a pressure-velocity formulation
% Constant viscosity case:

clear; clc;
addpath(genpath('/home/bkhanal/Documents/softwares/matlabTools/'));
addpath(genpath('/home/bkhanal/works/AdLemModel/matlab/modelInMatlab/'));

% Numbers of nodes
xn    =   13;             % Horizontal
yn    =   13;             % Vertical
N = 3*xn*yn;
% Coordinate values:
[x y] = meshgrid(1:xn,1:yn);
y = flipud(y);
% Boundary conditions:
nWallVx = 1;
nWallVy = 0;
sWallVx = 0;
sWallVy = 0;
wWallVx = 0;
wWallVy = 0;
eWallVx = 0;
eWallVy = 0;
% Boundary coefficient scale factors
dbScale = 1;
% Call a function:
brain_mask = ones(yn,xn);
% center_mask = [round(xn/2) round(yn/2)];
% brain_mask = circleMask(center_mask,xnum/4,xnum-1,ynum-1);

% atrophy at the center of the cells
a = zeros(yn,xn);
% a(5,5) = 0.5;
% a(10,10) = -0.5;
% a(sub2ind([yn xn],5:8,5:8)) = 0.5;
% a = computeAtrophy(yn-1,xn-1);

% Grid step
xstp    =   1;  %xsize/(xn-1); % Horizontal
ystp    =   1;  %ysize/(yn-1); % Vertical

% Computing Kcont and Kbond coefficients
kcont   =   1;
kbond   =   1;

% Pin pressure point: IMP: It cannot be any of the south or west wall or
% top right corner pressure vars since they are fictious cells here!
p0Cellx = 2; %indx j
p0Celly = 2; %indx i

% Process all Grid points
M = zeros(N);
R = zeros(N,1);
% maximum num of non zeros per row:
nnz = 7;
rowSt = struct('c',0,'i',0,'j',0,'k',0,'xn',xn,'yn',yn,'zn',1,'dof',3);
colSts(1) = rowSt;
for i=1:nnz
    colSts(i) = rowSt;
end
v = zeros(1,nnz);

for j=1:1:yn
    for i=1:1:xn
        rowSt.i = i;     rowSt.j = j;
        
% -------------------------------------------------------------------------        
        % x-Stokes equation: d2vx/dx^2
        rowSt.c = 0;    num = 1;
        row = getPos2D(rowSt);
        % (vx(i-1,j) + vx(i+1,j))/dx^2
        if(i>1)
            colSts(num).i = i-1;  colSts(num).j = j;
            v(num) = 1/(xstp*xstp); colSts(num).c = 0;
            num = num+1;
        end
        if(i<xn)
            colSts(num).i = i+1;  colSts(num).j = j;
            v(num) = 1/(xstp*xstp); colSts(num).c = 0;
            num = num+1;
        end
        % (vx(i,j-1) + vx(i,j+1))/dy^2
        if(j>1)
            colSts(num).i = i;  colSts(num).j = j-1;
            v(num) = 1/(ystp*ystp); colSts(num).c = 0;
            num = num+1;
        end
        if(j<yn)
            colSts(num).i = i;  colSts(num).j = j+1;
            v(num) = 1/(ystp*ystp); colSts(num).c = 0;
            num = num+1;
        end
        % -vx(i,j)/(dx^2+dy^2)
        colSts(num).i = i;  colSts(num).j = j;
        v(num) = -2/(ystp*ystp) - 2/(xstp*xstp); colSts(num).c = 0;
        num = num+1;
        
        % -dp/dx: (-p(i+1,j)+p(i,j))/xstp
        if(i<xn)
            colSts(num).i = i+1;  colSts(num).j = j;
            v(num) = -1/xstp;        colSts(num).c = 2;
            num = num+1;
        end
        colSts(num).i = i;  colSts(num).j = j;
        v(num) = 1/xstp;    colSts(num).c = 2;
        
        M(sub2ind([N N],row*ones(1,num),getPos2D(colSts(1:num)))) = v(1:num);
        
        %RESET the rows and explicitly set boundary conditions. 
        if(i==xn)   %East wall: First coz it sets things without interpolation.
            M(row,:) = 0;
            colSts(1).i = i;    colSts(1).j = j;    colSts(1).c = 0;
            M(row,getPos2D(colSts(1))) = dbScale;
        elseif(j==1)        %South wall:
            M(row,:) = 0;
            colSts(1).i = i;    colSts(1).j = j;
            v(1) = 3*dbScale;   colSts(1).c = 0;
            colSts(2).i = i;    colSts(2).j = j+1;  
            v(2) = -1*dbScale;  colSts(2).c = 0;
            M(sub2ind([N N],row*ones(1,2),getPos2D(colSts(1:2)))) = v(1:2);
        elseif(i==1)    %West wall: 
            M(row,:) = 0;
            colSts(1).i = i;    colSts(1).j = j;
            v(1) = 2*dbScale;   colSts(1).c = 0;
            colSts(2).i = i+1;  colSts(2).j = j;  
            v(2) = -1*dbScale;  colSts(2).c = 0;
            M(sub2ind([N N],row*ones(1,2),getPos2D(colSts(1:2)))) = v(1:2);
        elseif(j==yn)   %North wall:
            M(row,:) = 0;
            colSts(1).i = i;    colSts(1).j = j;
            v(1) = 3*dbScale;   colSts(1).c = 0;
            colSts(2).i = i;    colSts(2).j = j-1;  
            v(2) = -1*dbScale;  colSts(2).c = 0;
            M(sub2ind([N N],row*ones(1,2),getPos2D(colSts(1:2)))) = v(1:2);
        end        
        
        % RHS: first normal interior values:
        if(j==1)    %south wall
            R(row,1) = dbScale*2*sWallVx/3;
        elseif(i==1)    %west wall
            R(row,1) = dbScale*wWallVx;
        elseif(j==yn)   %north wall
            R(row,1) = dbScale*2*nWallVx;
        elseif(i==xn)   %east wall
            R(row,1) = dbScale*eWallVx;
        else
            R(row,1) = (a(i+1,j) - a(i,j))/xstp;
        end
% -------------------------------------------------------------------------        
        % y-Stokes equation
        rowSt.c = 1;    num = 1;
        row = getPos2D(rowSt);
        % (vy(i-1,j) + vy(i+1,j))/dx^2
        if(i>1)
            colSts(num).i = i-1;  colSts(num).j = j;
            v(num) = 1/(xstp*xstp); colSts(num).c = 1;
            num = num+1;
        end
        if(i<xn)
            colSts(num).i = i+1;  colSts(num).j = j;
            v(num) = 1/(xstp*xstp);     colSts(num).c = 1;
            num = num+1;
        end
        % (vy(i,j-1) + vy(i,j+1))/dy^2
        if(j>1)
            colSts(num).i = i;  colSts(num).j = j-1;
            v(num) = 1/(ystp*ystp);     colSts(num).c = 1;
            num = num+1;
        end
        if(j<yn)
            colSts(num).i = i;  colSts(num).j = j+1;
            v(num) = 1/(ystp*ystp);     colSts(num).c = 1;
            num = num+1;
        end
        % -vy(i,j)/(dx^2+dy^2)
        colSts(num).i = i;  colSts(num).j = j;
        v(num) = -2/(ystp*ystp) - 2/(xstp*xstp);     colSts(num).c = 1;
        num = num+1;
        
        % -dp/dy: -(p(i,j+1)+p(i,j))/ystp
        if(j<yn)
            colSts(num).i = i;  colSts(num).j = j+1;
            v(num) = -1/ystp;    colSts(num).c = 2;
            num = num+1;
        end
        colSts(num).i = i;  colSts(num).j = j;
        v(num) = 1/ystp;    colSts(num).c = 2;
        
        M(sub2ind([N N],row*ones(1,num),getPos2D(colSts(1:num)))) = v(1:num);
        
        %RESET the rows and explicitly set boundary conditions. 
        if(j==yn)   %North wall: (First because it sets directly not with interpolation
            M(row,:) = 0;
            colSts(1).i = i;    colSts(1).j = j;    colSts(1).c = 1;
            M(row,getPos2D(colSts(1))) = dbScale;
        elseif(j==1)        %South wall:
            M(row,:) = 0;
            colSts(1).i = i;    colSts(1).j = j;
            v(1) = 2*dbScale;   colSts(1).c = 1;
            colSts(2).i = i;    colSts(2).j = j+1;  
            v(2) = -1*dbScale;  colSts(2).c = 1;
            M(sub2ind([N N],row*ones(1,2),getPos2D(colSts(1:2)))) = v(1:2);
        elseif(i==1)    %West wall: 
            M(row,:) = 0;
            colSts(1).i = i;    colSts(1).j = j;
            v(1) = 3*dbScale;   colSts(1).c = 1;
            colSts(2).i = i+1;  colSts(2).j = j;  
            v(2) = -2*dbScale;  colSts(2).c = 1;
            M(sub2ind([N N],row*ones(1,2),getPos2D(colSts(1:2)))) = v(1:2);
        elseif(i==xn)   %East wall:
            M(row,:) = 0;
            colSts(1).i = i;    colSts(1).j = j;
            v(1) = 3*dbScale;   colSts(1).c = 1;
            colSts(2).i = i-1;    colSts(2).j = j;  
            v(2) = -1*dbScale;  colSts(2).c = 1;
            M(sub2ind([N N],row*ones(1,2),getPos2D(colSts(1:2)))) = v(1:2);
        end        
        
        % RHS: Note the same order of if-elseif as above!!
        if(j==yn)   %north wall
            R(row,1) = dbScale*nWallVy;
        elseif(j==1)    %south wall
            R(row,1) = dbScale*sWallVy;
        elseif(i==1)    %west wall
            R(row,1) = dbScale*2*wWallVy;
        elseif(i==xn)   %east wall
            R(row,1) = dbScale*2*eWallVy;
        else
            R(row,1) = (a(i,j+1) - a(i,j))/ystp;
        end
        
% ------------------------------------------------------------------------        
        %continuity equation: dvx/dx + dvy/dy =(vx(i,j)-vx(i-1,j))/dx + ...
        rowSt.c = 2;        num = 1;
        row = getPos2D(rowSt);
        %                         -vx(i-1,j)
        if(i>1)
            colSts(num).i = i-1;  colSts(num).j = j;
            v(num) = -1/xstp;      colSts(num).c = 0;
            num = num+1;
        end
        %vx(i,j)
        colSts(num).i = i;    colSts(num).j = j;
        v(num) = 1/xstp;      colSts(num).c = 0;
        num = num+1;
        
        %dvy/dy = (vy(i,j) - vy(i,j-1))/dy
        %-vy(i,j-1)
        if(j>1)
            colSts(num).i = i;  colSts(num).j = j-1;
            v(num) = -1/ystp;    colSts(num).c = 1;
            num = num+1;
        end
        %vy(i,j)
        colSts(num).i = i;    colSts(num).j = j;
        v(num) = 1/ystp;      colSts(num).c = 1;
        
        M(sub2ind([N N],row*ones(1,num),getPos2D(colSts(1:num)))) = v(1:num);
        

        % Boundary conditions:
        % pressure points corresponding to j=1(near south wall) and i=1
        % (near west wall) never featured in momentum equations! so, if we
        % put simply continuity equations to these grid points here, these
        % p variables do not have any equations to describe them. They will
        % thus make the rank-deficient matrix. Thus, instead of continuity
        % equations, for these points, we need to somehow constrain the
        % pressure variables. The choice is to either set them to certain
        % values as Dirichlet values or constrain them to have zero
        % gradient with Neumann condition.
        
        % Neumann condition on south and west wall
        if(j==1)    %South wall
            M(row,:) = 0;
            colSts(1).i = i;    colSts(1).j = j;
            v(1) = -dbScale;   colSts(1).c = 2;
            colSts(2).i = i;    colSts(2).j = j+1;  
            v(2) = dbScale;  colSts(2).c = 2;
            M(sub2ind([N N],row*ones(1,2),getPos2D(colSts(1:2)))) = v(1:2);
        elseif(i==1) %West wall
            M(row,:) = 0;
            colSts(1).i = i;    colSts(1).j = j;
            v(1) = -dbScale;   colSts(1).c = 2;
            colSts(2).i = i+1;    colSts(2).j = j;  
            v(2) = dbScale;  colSts(2).c = 2;
            M(sub2ind([N N],row*ones(1,2),getPos2D(colSts(1:2)))) = v(1:2);
        elseif(i==xn && j==yn) %top-corner
            M(row,:) = 0;
            colSts(1).i = i;    colSts(1).j = j;
            v(1) = -dbScale;   colSts(1).c = 2;
            colSts(2).i = i-1;    colSts(2).j = j;  %we have choice here,
            %with (i-1,j) or (i,j-1). 
            v(2) = dbScale;  colSts(2).c = 2;
            M(sub2ind([N N],row*ones(1,2),getPos2D(colSts(1:2)))) = v(1:2);
        elseif(i==p0Cellx && j==p0Celly)
            M(row,:) = 0;
            M(row,row) = dbScale;
        end
        
        % RHS
        if(i==p0Cellx && j==p0Celly)         % Fixed point:
            R(row,1) = 0;
        elseif(j==1) %South wall
            R(row,1) = 0;
        elseif(i==1) %West wall
            R(row,1) = 0;
        elseif(i==xn && j==yn) %top-corner
            R(row,1) = 0;
        else
            R(row,1) = -a(i,j);
        end
    end
end

% rank(M)
% Test number of non-zeros in each row:
% nnz = zeros(27); nnz(M==0) = 1; nnz = sum(~nnz,2);
% Solution
sol = M\R;
res = M*sol - R;
vx = reshape(sol(1:3:end),xn,yn);
vy = reshape(sol(2:3:end),xn,yn);
p = rot90(reshape(sol(3:3:end),xn,yn));
imagesc(p);

% Let's interpolate the values: 
vxC = (vx(1:xn-1,2:yn) + vx(2:xn,2:yn))/2;
vyC = (vy(2:xn,1:yn-1) + vy(2:xn,2:yn))/2;
vxC = padarray(vxC,[1 1],0,'pre');
vyC = padarray(vyC,[1 1],0,'pre');
vxC = rot90(vxC);   vyC = rot90(vyC);
figure, quiver(x,y,vxC,vyC);
vx = rot90(vx); vy = rot90(vy);
%% Test Null Space
nullBasis = zeros(N,1);
nullBasis(3:3:end) = 1;
r = M*nullBasis;

%% Test Schur complement:
RM = reorderInterLeavedToBlockwise3Dof(M);
A = RM(1:2*xn*yn,1:2*xn*yn);
B = RM(1:2*xn*yn,2*xn*yn+1:end);
D = RM(2*xn*yn+1:end,1:2*xn*yn);
nnz = B'-D;
S = B'*(A\B);
%%Solution:



%% Null spaces:
[u D v] = svd(M);

currV=v(:,end-1);
nullP = rot90(reshape(currV(3:3:end),xn,yn));
imagesc(nullP);
nullVx = rot90(reshape(currV(1:3:end),xn,yn));
nullVy = rot90(reshape(currV(2:3:end),xn,yn));
figure, quiver(x,y,nullVx,nullVy);

%% Test Quiver plots
vxTest = zeros(yn,xn); vyTest = zeros(yn,xn);
posSt.xn = xn; posSt.yn = yn; posSt.dof = 1;
for j=1:yn
    for i = 1:xn
        posSt.i = i;  posSt.j = j;  posSt.c = 0; 
        pos = getPos2D(posSt);
        vxTest(i,j) = i;
        vyTest(i,j) = j;
    end
end
vxTest = rot90(vxTest); vyTest = rot90(vyTest);
quiver(x,y,vxTest,vyTest);

%% Test divergence of the solution
divVal = zeros(yn,xn);
for i=1:2
    vxPos(i).xn = xn; vxPos(i).yn = yn; vxPos(i).dof = 3; vxPos(i).c = 0; 
    vyPos(i).xn = xn; vyPos(i).yn = yn; vyPos(i).dof = 3; vyPos(i).c = 1;
end
for j=1:yn
    for i = 1:xn
        vxPos(1).i = i;  vxPos(1).j = j;  
        vyPos(1).i = i;  vyPos(1).j = j;  
        
        divVal(i,j) = sol(getPos2D(vxPos(1))) + sol(getPos2D(vyPos(1)));
        if(i>1)
            vxPos(2).i = i-1; vxPos(2).j = j;
            divVal(i,j) = divVal(i,j) - sol(getPos2D(vxPos(2)));
        end
        if(j>1)
            vyPos(2).i = i; vyPos(2).j = j-1;
            divVal(i,j) = divVal(i,j) - sol(getPos2D(vyPos(2)));
        end
    end
end
divVal = rot90(divVal); 
imagesc(divVal);