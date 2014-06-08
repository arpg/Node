%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%% MATLAB class wrapper to the Node C++ architecture.
%%%% Any method with 'MatNodeExample(...)'
%%%% represents a call to the Node wrapper for MATLAB.
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

classdef MatNode < handle
  properties (SetAccess = public)
    sims_;         % Handle to the node instance
    num_sims_;
  end
  
  methods
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    %%%% CONSTRUCTOR/DESTRUCTOR
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    
    function this = MatNode(num_sims)
    % This initializes the class for us. Don't skip this line
      this.sims_ = MatNodeExample('new', num_sims);
      this.num_sims_ = num_sims;
    end
    
    function delete(this)
      MatNodeExample('delete', this.sims_);
    end
    
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    %%%% OUR FUNCTIONS
    %%%% For demo purposes, I've wrapped both functions in the .cpp
    %%%% file into one MATLAB function. 
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    
    function Run(this)
        disp('[MATLAB] Starting Sim connections...');
        MatNodeExample('StartConnections', this.sims_, ...
                       this.num_sims_);
        while 1, 
            for i=0:this.num_sims_-1, 
                [status] = MatNodeExample('CheckSimStatus', this.sims_, ...
                                          i);
                disp(['[MATLAB] Sim', num2str(i), ' status: ', ...
                      num2str(status)]);
            end
        end 
    end
    
  end
end
