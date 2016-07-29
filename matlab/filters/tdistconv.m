function [I, G] = tdistconv(I, sigma, G)

    if nargin < 2; sigma = 1; end
    if nargin < 3;
        % create Student-T filter for I
        
        [h, w, ~] = size(I);

        % multivariate implementation
        nu = 2;
        Rho = [1 0; 0 1];

        cw = (floor(w/2)+1);
        ch = (floor(h/2)+1);
        di = [1:w] - cw;
        dj = [1:h] - ch;
        di = di / sigma;
        dj = dj / sigma;
        [X1, X2] = meshgrid(di, dj);
        G = mvtpdf([X1(:) X2(:)], Rho, nu);
        G = G ./ max(G(:));
        %G = mvnpdf([X1(:) X2(:)], [0, 0], Rho);
        G = reshape(G, [h w]);
    end

    % perform convolution
    if 0
        for j = 1:size(I,3)
            I(:,:,j) = conv2(I(:,:,j), G, 'same');
        end
    else
        % faster
        I = conv2_fft2(I, G);
    end
    
end

function Y = conv2_fft2(X, G)
    fX = X;
    fG = G;
    
    fX = ifftshift(fX,1);
    fX = ifftshift(fX,2);
    fG = ifftshift(fG,1);
    fG = ifftshift(fG,2);
    
    fX = fft2(fX);
    fY = fft2(fG);
    fY = bsxfun(@times, fX, fY); % broadcast
    
    fY = ifft2(fY);
    fY = real(fY);
    
    fY = fftshift(fY,1);
    fY = fftshift(fY,2);
    Y = fY;
end
