function OpenFrescoExpress(action)
%OPENFRESCOEXPRESS to launch OpenFrescoExpress and display the disclaimer
% OpenFrescoExpress(action)
%
% action : switch with following possible values
%             'startup'  to show disclaimer during startup
%             'about'    to show disclaimer from the help menu
%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%                          OpenFresco Express                          %%
%%    GUI for the Open Framework for Experimental Setup and Control     %%
%%                                                                      %%
%%   (C) Copyright 2011, The Pacific Earthquake Engineering Research    %%
%%            Center (PEER) & MTS Systems Corporation (MTS)             %%
%%                         All Rights Reserved.                         %%
%%                                                                      %%
%%     Commercial use of this program without express permission of     %%
%%                 PEER and MTS is strictly prohibited.                 %%
%%     See Help -> OpenFresco Express Disclaimer for information on     %%
%%   usage and redistribution, and for a DISCLAIMER OF ALL WARRANTIES.  %%
%%                                                                      %%
%%   Developed by:                                                      %%
%%     Andreas Schellenberg (andreas.schellenberg@gmail.com)            %%
%%     Carl Misra (carl.misra@gmail.com)                                %%
%%     Stephen A. Mahin (mahin@berkeley.edu)                            %%
%%                                                                      %%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
% $Revision$
% $Date$
% $URL$

if (nargin==0)
    action = 'startup';
end

if ~isempty(findobj('Tag','Disclaimer'))
   figure(findobj('Tag','Disclaimer'));
   return
end

% text for disclaimer
TEXT = ['\nThe Pacific Earthquake Engineering Research Center (PEER), MTS Systems ' ...
        'Corporation (MTS) and the authors provide this software without a signed ' ...
        'license. It is distributed by PEER and MTS "as is," as a public service, ' ...
        'without any additional services from, or licensing by, PEER and MTS. The ' ...
        'program has been developed by researchers at PEER. PEER, MTS and the authors ' ...
        'do not warrant that the operation of the program will be uninterrupted or ' ...
        'error-free.\n\n' ...
        'Please acknowledge in the acknowledgment section of any resulting publications '...
        'use of OpenFresco Express.\n\n' ...
        'IN NO EVENT SHALL PEER OR MTS OR THE AUTHORS BE LIABLE FOR PERSONAL INJURY, ' ...
        'OR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ' ...
        'WHATSOEVER, INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOST PROFITS, LOSS OF ' ...
        'DATA BUSINESS INTERRUPTION OR ANY OTHER COMMERCIAL DAMAGES OR LOSSES ARISING ' ...
        'OUT OF OR RELATED TO THE USE OR INABILITY TO USE THE SOFTWARE, HOWEVER CAUSED, ' ...
        'REGARDLESS OF THE THEORY OF LIABILITY AND EVEN IF PEER OR MTS OR THE AUTHORS ' ...
        'HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.\n\n' ...
        'PEER, MTS AND THE AUTHORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING BUT ' ...
        'NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A ' ...
        'PARTICULAR PURPOSE. NEITHER PEER NOR MTS NOR THE AUTHORS REPRESENT OR WARRANT ' ...
        'THAT THE PROGRAM DOES NOT INFRINGE THE INTELLECTUAL PROPERTY, PROPRIETARY OR ' ...
        'CONTRACTUAL RIGHTS OF THIRD PARTIES. THE SOFTWARE PROVIDED HEREUNDER IS ON AN ' ...
        '"AS IS" BASIS, AND PEER AND MTS AND THE AUTHORS HAVE NO OBLIGATIONS TO PROVIDE ' ...
        'MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS. \n\n' ...
        'SERVOHYDRAULIC TEST SYSTEMS ARE DESIGNED FOR MAXIMUM FORCE AND VELOCITY ' ...
        'PERFORMANCE. THEY SHOULD BE CONSIDERED HAZARDOUS WITH APPROPRIATE SAFETY ' ...
        'PRECAUTIONS TAKEN FOR OPERATORS.\n'];

% disclaimer screen size
SS = get(0,'screensize');
Width = 0.5*SS(3);
Height = 0.5*SS(4);

% create window
figure('Name','Disclaimer', ...
    'NumberTitle','off', ...
    'Color',[0.3 0.5 0.7], ...
    'MenuBar','none', ...
    'Tag','Disclaimer', ...
    'Position',[0.303*SS(3) 0.258*SS(4) Width Height]);
uicontrol('Style','text', ...
    'Units','pixels', ...
    'Position',[0 0.89*Height Width 0.05*Height], ...
    'String','Disclaimer', ...
    'FontUnits','pixels','FontWeight','normal','FontSize',25,'FontName','Arial', ...
    'BackgroundColor',[0.3 0.5 0.7],'ForegroundColor','w','Horizontal','center');
uicontrol('Style','edit', ...
    'Max',10,'Min',1, ...
    'Units','pixels', ...
    'Position',[0.075*Width 0.2*Height 0.85*Width 0.65*Height], ...
    'String',sprintf(TEXT), ...
    'FontUnits','pixels','FontWeight','bold','FontSize',0.025*Height, ...
    'FontName','Arial','BackgroundColor','w','ForegroundColor','k', ...
    'Horizontal','left');

switch action
    %=========================================================================
    case 'startup'
        % Accept button
        callbackStr = 'close(findobj(''Tag'',''Disclaimer'')); GUI_Template;';
        uicontrol('Style','pushbutton', ...
            'Units','pixels', ...
            'Position',[0.3*Width-0.2*Width/2 0.05*Height 0.2*Width 0.075*Height], ...
            'String','I accept', ...
            'FontUnits','pixels','FontWeight','bold','FontSize',0.025*Height,...
            'Horizontal','center','Callback',callbackStr);
        
        % Decline button
        callbackStr = 'close(findobj(''Tag'',''Disclaimer''));';
        uicontrol('Style','pushbutton', ...
            'Units','pixels', ...
            'Position',[0.7*Width-0.2*Width/2 0.05*Height 0.2*Width 0.075*Height], ...
            'String','I decline', ...
            'FontUnits','pixels','FontWeight','bold','FontSize',0.025*Height,...
            'Horizontal','center','Callback',callbackStr);
    %=========================================================================
    case 'about'
        % Close button
        callbackStr = 'close(findobj(''Tag'',''Disclaimer''));';
        uicontrol('Style','pushbutton', ...
            'Units','pixels', ...
            'Position',[0.5*Width-0.2*Width/2 0.05*Height 0.2*Width 0.075*Height], ...
            'String','close', ...
            'FontUnits','pixels','FontWeight','bold','FontSize',0.025*Height,...
            'Horizontal','center','Callback',callbackStr);
   %=========================================================================
end

%#function Analysis AnimateResponse AxesHelp DisplayAxes 
%#function Element_ExpGeneric ExpControl ExpSetup GUI_AnalysisControls 
%#function GUI_AnimateStructure GUI_ErrorMonitors GUI_Output GUI_StructuralOutput 
%#function GUI_Template GetFFT GetFileList GroundMotions 
%#function InputCheck Integrator_AlphaOS Integrator_NewmarkExplicit Links 
%#function MenuBar OpenFrescoExpress ReadWriteTHFileNGA ReplayResults 
%#function Report ResizeImage RunOpenFresco SetOPFPath 
%#function SplashScreen Structure TCPSocket TerminateAnalysis 
%#function UpdatePragmaList WriteTCL 